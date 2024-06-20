# Owner(s): ["oncall: distributed"]


import contextlib
import itertools
import unittest

import torch
import torch._dynamo.testing
from torch import nn
from torch._dynamo import compiled_autograd

from torch.distributed._composable.fsdp import fully_shard
from torch.distributed._composable.fsdp._fsdp_common import TrainingState
from torch.distributed._composable.fsdp._fsdp_init import (
    _get_managed_modules,
    _get_managed_states,
)
from torch.distributed._composable.fsdp._fsdp_param_group import FSDPParamGroup
from torch.distributed._tensor import init_device_mesh
from torch.testing._internal.common_distributed import skip_if_lt_x_gpu
from torch.testing._internal.common_fsdp import FSDPTest, MLP
from torch.testing._internal.common_utils import run_tests
from torch.testing._internal.distributed._tensor.common_dtensor import ModelArgs, Transformer
from torch.utils._triton import has_triton


class TestFullyShardCompileCompute(FSDPTest):
    @unittest.skipIf(not has_triton(), "Inductor+gpu needs triton and recent GPU arch")
    @skip_if_lt_x_gpu(2)
    def test_disable_compiling_hooks(self):
        self.run_subtests(
            {
                "skip_fsdp_hooks": [False, True],
            },
            self._test_disable_compiling_hooks,
        )

    def _test_disable_compiling_hooks(
        self,
        skip_fsdp_hooks: bool,
    ):
        torch._dynamo.reset()
        trace_rules_check_count = 0
        HOOKS_FILE_NAME = "torch/distributed/_composable/fsdp/_fsdp_state.py"
        HOOK_WRAPPER_NAME = "fsdp_hook_wrapper"

        def patched_trace_rules_check(*args, **kwargs):
            nonlocal trace_rules_check_count
            f_code = args[0]
            if (
                hasattr(f_code, "co_filename")
                and f_code.co_filename.endswith(HOOKS_FILE_NAME)
                and f_code.co_name != HOOK_WRAPPER_NAME
            ):
                trace_rules_check_count += 1
            return orig_trace_rules_check(*args, **kwargs)

        original_skip_fsdp_hooks = torch._dynamo.config.skip_fsdp_hooks
        orig_trace_rules_check = torch._dynamo.trace_rules.check
        torch.distributed.barrier()
        torch._dynamo.config.skip_fsdp_hooks = skip_fsdp_hooks
        torch._dynamo.trace_rules.check = patched_trace_rules_check
        model = MLP(4)
        fully_shard(model)
        model.compile()
        model(torch.randn((4, 4), device="cuda"))
        torch.distributed.barrier()
        torch._dynamo.config.skip_fsdp_hooks = original_skip_fsdp_hooks
        torch._dynamo.trace_rules.check = orig_trace_rules_check
        if skip_fsdp_hooks:
            self.assertEqual(trace_rules_check_count, 0)
        else:
            self.assertTrue(trace_rules_check_count > 0)


def cleanup_fsdp(model):
    # This is important for releasing memory of all tensors used in the FSDP-wrapped modules.
    torch._dynamo.reset()
    for state in torch.distributed._composable_state._module_state_mapping.values():
        if hasattr(state._fsdp_param_group, "fsdp_params"):
            for fsdp_param in state._fsdp_param_group.fsdp_params:
                fsdp_param._sharded_param_data.untyped_storage().resize_(0)
    managed_modules = _get_managed_modules(model)
    params, buffers = _get_managed_states(managed_modules)
    for tensor in itertools.chain(params, buffers):
        try:
            tensor.untyped_storage().resize_(0)
        except Exception:
            pass
    torch.distributed._composable_state._module_state_mapping = {}


class TestFullyShardCompile(FSDPTest):
    @property
    def world_size(self) -> int:
        return min(2, torch.cuda.device_count())

    def test_dynamo_trace_use_training_state(self):
        torch._dynamo.reset()
        # Construct a dummy FSDPParamGroup, since we just want to test the `use_training_state` ctx manager.
        param_group = FSDPParamGroup(
            [],  # params: List[nn.Parameter],
            torch.nn.Linear(1, 1),  # module: nn.Module,
            None,  # mesh_info: FSDPMeshInfo,
            None,  # post_forward_mesh_info: Optional[FSDPMeshInfo],
            None,  # device: torch.device,
            None,  # mp_policy: MixedPrecisionPolicy,
            None,  # offload_policy: OffloadPolicy,
        )

        def f(x):
            param_group._training_state = TrainingState.IDLE
            with param_group.use_training_state(TrainingState.FORWARD):
                if param_group._training_state == TrainingState.FORWARD:
                    return x + 1
                else:
                    return x

        inp = torch.zeros(1)
        self.assertEqual(param_group._training_state, TrainingState.IDLE)

        eager_out = f(inp)
        self.assertEqual(param_group._training_state, TrainingState.IDLE)
        self.assertEqual(eager_out, inp + 1)

        cnt = torch._dynamo.testing.CompileCounterWithBackend("aot_eager")
        compiled_out = torch.compile(f, backend=cnt, fullgraph=True)(inp)
        self.assertEqual(param_group._training_state, TrainingState.IDLE)
        self.assertEqual(eager_out, compiled_out)
        self.assertEqual(cnt.frame_count, 1)
        self.assertEqual(cnt.op_count, 1)
        self.assertEqual(len(cnt.graphs), 1)

    def _test_traceable_fsdp(
        self, model_init_fn, input_creation_fn, backend, fullgraph
    ):
        n_iter = 10

        def compiler_fn(compiled_autograd_backend):
            def _fn(gm):
                # fullgraph=True because graph-break in Compiled Autograd BWD graph is not supported by Traceable FSDP2 yet
                # (main difficulty comes from queue_callback not working well when BWD has graph break).
                return torch.compile(gm, backend=compiled_autograd_backend, fullgraph=True)

            return _fn

        def run_all_iters(model, optim, compiled_autograd_backend=None):
            torch.manual_seed(42)
            losses = []
            for i in range(n_iter):
                optim.zero_grad(set_to_none=True)
                inp = input_creation_fn()
                if compiled_autograd_backend is not None:
                    maybe_compiled_autograd_ctx = compiled_autograd.enable(
                        compiler_fn(compiled_autograd_backend)
                    )
                else:
                    maybe_compiled_autograd_ctx = contextlib.nullcontext()
                with maybe_compiled_autograd_ctx:
                    out = model(inp)
                    loss = out.sum()
                    losses.append(loss.item())
                    loss.backward()
                optim.step()
                del loss
                del out
                del inp
                torch.cuda.synchronize()
            return losses

        def test_compiled():
            model, optim = model_init_fn()
            # FSDP2 does lazy init using 1st run, so run it once to init using eager mode
            run_all_iters(model, optim, 1)

            model_compiled = torch.compile(model, backend=backend, fullgraph=True)
            res = run_all_iters(model_compiled, optim, compiled_autograd_backend=backend)
            cleanup_fsdp(model)
            del model_compiled
            del model
            optim.zero_grad(set_to_none=True)
            del optim
            return res

        def test_eager():
            model, optim = model_init_fn()
            # FSDP2 does lazy init using 1st run, so run it once to init using eager mode
            run_all_iters(model, optim, 1)

            res = run_all_iters(model, optim)
            cleanup_fsdp(model)
            del model
            optim.zero_grad(set_to_none=True)
            del optim
            return res

        losses_compiled = test_compiled()
        losses_eager = test_eager()
        for loss_compiled, loss_eager in zip(losses_compiled, losses_eager):
            self.assertTrue(
                torch.allclose(
                    torch.tensor(loss_compiled), torch.tensor(loss_eager), rtol=1e-3
                ),
                f"{loss_compiled} vs {loss_eager}",
            )

    def _create_simple_mlp_factory_fns(self):
        hidden_dim = 16

        def model_init_fn():
            torch.manual_seed(0)
            fsdp_config = {}
            model = nn.Sequential(
                nn.Linear(hidden_dim, hidden_dim, device="cuda"),
                nn.ReLU(),
                nn.Linear(hidden_dim, hidden_dim, device="cuda"),
                nn.ReLU(),
                nn.Linear(hidden_dim, hidden_dim, device="cuda"),
            )
            fully_shard(model, reshard_after_forward=True, **fsdp_config)
            optim = torch.optim.SGD(model.parameters(), lr=1e-6)
            return model, optim

        def input_creation_fn():
            torch.manual_seed(0)
            inp = torch.randn((2, hidden_dim), device="cuda", requires_grad=False)
            return inp

        return model_init_fn, input_creation_fn

    @skip_if_lt_x_gpu(2)
    def test_simple_mlp_fullgraph_backend_eager(self):
        self._test_traceable_fsdp(
            *self._create_simple_mlp_factory_fns(), "eager", fullgraph=True
        )

    @skip_if_lt_x_gpu(2)
    def test_simple_mlp_fullgraph_backend_aot_eager(self):
        self._test_traceable_fsdp(
            *self._create_simple_mlp_factory_fns(), "aot_eager", fullgraph=True
        )

    @unittest.expectedFailure
    @unittest.skipIf(not has_triton(), "Inductor+gpu needs triton and recent GPU arch")
    @skip_if_lt_x_gpu(2)
    def test_simple_mlp_fullgraph_backend_inductor(self):
        self._test_traceable_fsdp(
            *self._create_simple_mlp_factory_fns(), "inductor", fullgraph=True
        )

    def _create_transformer_factory_fns(self):
        hidden_dim = 16

        def model_init_fn():
            torch.manual_seed(0)
            fsdp_config = {}
            mesh = init_device_mesh("cuda", (self.world_size,))
            model_args = ModelArgs(
                dim=hidden_dim,
                n_layers=2,
                n_heads=1,
                vocab_size=1024,
            )
            model = Transformer(model_args)
            for layer_id, mod in enumerate(model.layers):
                fully_shard(mod, mesh=mesh, reshard_after_forward=True, **fsdp_config)
                model.layers[layer_id] = mod
            model = fully_shard(
                model, mesh=mesh, reshard_after_forward=True, **fsdp_config
            )
            optim = torch.optim.SGD(model.parameters(), lr=1e-6)
            return model, optim

        def input_creation_fn():
            torch.manual_seed(0)
            inp = torch.zeros(
                (2, hidden_dim),
                device="cuda",
                requires_grad=False,
                dtype=torch.long,
            )
            return inp

        return model_init_fn, input_creation_fn

    @skip_if_lt_x_gpu(2)
    def test_transformer_fullgraph_backend_eager(self):
        self._test_traceable_fsdp(
            *self._create_transformer_factory_fns(), "eager", fullgraph=True
        )

    @skip_if_lt_x_gpu(2)
    def test_transformer_fullgraph_backend_aot_eager(self):
        self._test_traceable_fsdp(
            *self._create_transformer_factory_fns(), "aot_eager", fullgraph=True
        )

    @unittest.expectedFailure
    @unittest.skipIf(not has_triton(), "Inductor+gpu needs triton and recent GPU arch")
    @skip_if_lt_x_gpu(2)
    def test_transformer_fullgraph_backend_inductor(self):
        self._test_traceable_fsdp(
            *self._create_transformer_factory_fns(), "inductor", fullgraph=True
        )


if __name__ == "__main__":
    run_tests()
