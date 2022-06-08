import torch

from torch.utils._pytree import tree_map, tree_flatten
from functools import partial
from torch.fx.operator_schemas import normalize_function
from torch.utils._mode_utils import no_dispatch
from torch._subclasses.meta_utils import MetaConverter
from typing import Union
from torch._ops import OpOverload
from torch.utils._python_dispatch import TorchDispatchMode, enable_torch_dispatch_mode
import weakref
import functools
import itertools

aten = torch.ops.aten
prims = torch.ops.prims


class ComplexInputException(Exception):
    pass


_device_not_kwarg_ops = (
    aten._resize_output_.default,
    aten.nested_tensor.default,
    aten.pin_memory.default,
    aten.is_pinned.default,
    aten.to.device,
    aten.to.prim_Device,
    aten._pin_memory.default,
    aten._resize_output.functional,
    aten._resize_output.out,
)

# this op is never actually used
_non_kwarg_device_constructors = (torch.ops.aten._list_to_tensor,)

def contains_tensor_types(type):
    tensor_type = torch._C.TensorType.get()
    return type.isSubtypeOf(tensor_type) or any(
        contains_tensor_types(e) for e in type.containedTypes()
    )

_like_tensor_constructors = (
    aten.empty_like.default,
    aten.full_like.default,
    aten.ones_like.default,
    aten.rand_like.default,
    aten.randn_like.default,
    aten.randint_like.default,
    aten.randint_like.low_dtype,
    aten.randn_like.default,
    aten.zeros_like.default,
    aten.new_empty.default,
)

@functools.lru_cache(None)
def _is_tensor_constructor(func: OpOverload):
    assert isinstance(func, OpOverload)
    schema = func._schema
    if any(contains_tensor_types(arg.type) for arg in schema.arguments):
        return False
    # TODO: no real reason to restrict multiple outputs
    return (
        len(schema.returns) == 1 and schema.returns[0].type is torch._C.TensorType.get()
    )

# Similar to `MetaConverter`, this is a class for converting
# multiple tensors into fake tensors which share the same view/storage
# structure. Like `MetaConverter`, it will keep alive all
# tensors that are converted to FakeTensors.
class FakeTensorConverter(object):
    tensor_memo: weakref.WeakValueDictionary
    meta_converter: MetaConverter

    def __init__(self):
        # FakeTensors store the FakeTensorMode which in turn stores a
        # FakeTensor, so we need to hold a weak reference to the FakeTensor
        # otherwise we would induce a circular reference
        self.tensor_memo = weakref.WeakValueDictionary()
        self.meta_converter = MetaConverter()

    def _get_memo(self, t):
        if t in self.tensor_memo:
            out = self.tensor_memo[t]
            out._fix_weakref()
            return out
        return None

    def from_real_tensor(self, fake_mode, t):
        maybe_memo = self._get_memo(t)
        if maybe_memo is not None:
            return maybe_memo
        existing_device = t.device
        # not yet supported in metatensors
        if t.is_complex():
            raise ComplexInputException
        out = FakeTensor(fake_mode, self.meta_converter(t), existing_device)
        self.tensor_memo[t] = out
        return out

    def from_meta_and_device(self, fake_mode, t, device):
        maybe_memo = self._get_memo(t)
        if maybe_memo is not None:
            return maybe_memo
        out = FakeTensor(fake_mode, t, device)
        self.tensor_memo[t] = out
        return out

    def __call__(self, fake_mode, t, device=None):
        assert t.device.type != 'meta' or device is not None
        if t.device.type != 'meta':
            return self.from_real_tensor(fake_mode, t)
        else:
            return self.from_meta_and_device(fake_mode, t, device)


# Meta tensors give you the ability to run PyTorch code without having to
# actually do computation through tensors allocated on a `meta` device.
# Because the device is `meta`, meta tensors do not model device propagation.
# FakeTensor extends MetaTensors to also carry an additional `fake_device`
# which tracks devices that would have been used.

class FakeTensor(torch.Tensor):
    fake_device: torch.device
    fake_mode: "FakeTensorMode"

    @staticmethod
    def __new__(cls, fake_mode, elem, device):
        return torch.Tensor._make_subclass(
            cls, elem, elem.requires_grad, dispatch_device=True
        )

    def __init__(self, fake_mode, elem, device: Union[torch.device, str]):
        # elem does not need to be recorded, because FakeTensor *is a* elem
        assert elem.device.type == "meta"
        device = device if isinstance(device, torch.device) else torch.device(device)
        self.fake_device = device
        self.fake_mode = fake_mode

    @staticmethod
    def from_tensor(t, fake_mode):
        existing_device = t.device
        return FakeTensor(fake_mode, t.to(device="meta"), existing_device)

    # TODO: resolve error in default __repr__
    def __repr__(self):
        return f"FakeTensor({self.fake_device})"

    @classmethod
    def __torch_dispatch__(cls, func, types, args=(), kwargs=None):
        # need to handle here to avoid infinite recursion
        if func == torch.ops.prim.device.default:
            assert len(args) == 1 and isinstance(args[0], FakeTensor)
            return args[0].fake_device

        fake_mode = None
        for arg in itertools.chain(tree_flatten(args)[0], tree_flatten(kwargs)[0]):
            if isinstance(arg, FakeTensor):
                if fake_mode is None:
                    fake_mode = arg.fake_mode
                else:
                    assert fake_mode is arg.fake_mode, "Mixing modes NYI"

        with enable_torch_dispatch_mode(fake_mode):
            return func(*args, **kwargs)

    @staticmethod
    def _find_common_device(func, args, kwargs):
        # cpu - zero-dim tensors can be called in cuda kernels,
        # so overwrite the common_device if it the only existing
        # device comes from a cpu zero-dim tensor
        common_device = None
        is_cpu_zero_dim = None

        def cpu_zero_dim(t):
            return t.device.type == "cpu" and t.dim() == 0

        def merge_devices(t):
            nonlocal common_device
            nonlocal is_cpu_zero_dim
            if not isinstance(t, FakeTensor):
                return

            if common_device is None:
                common_device = t.device
                is_cpu_zero_dim = cpu_zero_dim(t)
                return

            t_is_cpu_zero_dim = cpu_zero_dim(t)
            if t.device == common_device:
                if is_cpu_zero_dim:
                    is_cpu_zero_dim = t_is_cpu_zero_dim
                return

            # mismatching devices !
            # if current tensor is cpu 0 dim, defer to existing device
            if t_is_cpu_zero_dim:
                return

            # current device is from cpu 0 dim tensor, overwrite
            if is_cpu_zero_dim:
                common_device = t.device
                is_cpu_zero_dim = t_is_cpu_zero_dim
                return

            # mismatching devices of non-zero dim tensors, throw
            # This might be valid behavior and need to be explicitly modeled, e.g. reshape_as
            raise Exception(
                f"Unhandled FakeTensor Device Propagation for {func}, found two different devices {common_device}, {t.device}"
            )

        tree_map(merge_devices, args)
        tree_map(merge_devices, kwargs)

        assert common_device is not None, f"Could not find common device for {func}"

        return common_device

    __torch_function__ = torch._C._disabled_torch_function_impl


# We keep one instantiation of `fake_tensor_converter` active
# for the duration of `with torch_enable_mode(FakeTensorMode)`.
# This allows accurate storage aliasing across invocation of
# different operators. While this will keep all freshly allocated
# tensors alive during `FakeTensorMode`, there will no be no
# new allocations of Tensors which have non-meta storage so
# memory should not significantly incraese.

class FakeTensorMode(TorchDispatchMode):
    def __init__(self):
        self.fake_tensor_converter = FakeTensorConverter()

    def __torch_dispatch__(self, func, types, args=(), kwargs=None):
        kwargs = kwargs if kwargs else {}

        # prims already wrap FakeTensor inputs to FakeTensor outputs
        # and do device logic, we dont need do anything but run them
        if "prims::" in func._schema.name:
            with no_dispatch():
                return func(*args, **kwargs)

        # TODO: apply as no_dispatch decorator
        with no_dispatch():
            converter = self.fake_tensor_converter

            def wrap(e, device=None):
                if isinstance(e, torch.Tensor) and not isinstance(e, FakeTensor):
                    return converter(self, e, device)
                else:
                    return e

            # if we are in the dispatch mode, we will enter this function even if the inputs
            # are not FakeTensors. For now, throw if any non-Fake Tensor inputs
            # and just support constructors. TODO: extend more broadly
            conversion_made = False

            def check_non_fake_tensor(x):
                nonlocal conversion_made
                conversion_made = conversion_made or (isinstance(x, torch.Tensor) and not isinstance(x, FakeTensor))

            tree_map(check_non_fake_tensor, args)
            tree_map(check_non_fake_tensor, kwargs)

            if conversion_made:
                raise Exception(
                    "Invoking operators with non-Fake Tensor inputs in FakeTensorMode is not yet supported. "
                    f"Please convert all Tensors to FakeTensors first. Found in {func}"
                )

            # _to_copy fails when run with FakeTensors to cuda device
            # TODO: debug
            if func == torch.ops.aten._to_copy.default:
                _, new_kwargs = normalize_function(
                    func, args=args, kwargs=kwargs, normalize_to_only_use_kwargs=True
                )

                out_device = new_kwargs.pop("device", new_kwargs["input"].device)
                with no_dispatch():
                    input = new_kwargs.pop("input").to("meta")
                    return FakeTensor(
                        self, torch.ops.aten._to_copy(input, **new_kwargs), out_device
                    )

            # TODO: cleaner prims support
            if (_is_tensor_constructor(func) or func in _like_tensor_constructors) \
                    and "prims::" not in func._schema.name:
                assert func not in _non_kwarg_device_constructors
                _, new_kwargs = normalize_function(
                    func, args=args, kwargs=kwargs, normalize_to_only_use_kwargs=True
                )
                if func in _like_tensor_constructors:
                    default_device = new_kwargs["input"].device
                    # TODO: file issue
                    args = (new_kwargs.pop("input"),)
                else:
                    # cpu is default device if none is specified
                    default_device = torch.device("cpu")
                    args = ()
                out_device = new_kwargs.pop("device")
                out_device = out_device if out_device else default_device
                new_kwargs["device"] = torch.device("meta")
                r = func(*args, **new_kwargs)
                return FakeTensor(self, r, out_device)

            if func in (aten.to.prim_Device, aten.to.device):
                _, new_kwargs = normalize_function(func, args, kwargs, normalize_to_only_use_kwargs=True)
                input_device = new_kwargs["device"]
                out_device = input_device if input_device else new_kwargs["input"].device
                new_kwargs["device"] = torch.device("meta")
                r = func(*args, **new_kwargs)
                return converter(self, r, out_device)

            # TODO: dont know why this is being dispatched to __torch__function__
            # Dont default to common device handling since this doesnt take in/return tensor
            # TODO: update typo of minium
            if func in (prims.maximum_value.default, prims.minium_value.default):
                return func(*args, **kwargs)

            r = func(*args, **kwargs)

            # TODO: handle non-kwarg devices
            assert func not in _device_not_kwarg_ops, f"NYI: {func}"

            # if device is specified, use that
            if kwargs.get("device", None):
                return tree_map(partial(wrap, device=kwargs["device"]), r)

            # operators which copy size from another tensor do not
            # also take device from the size tensor
            # other size_as operators are not builtin operators
            if func == aten.resize_as_.default:
                _, new_kwargs = normalize_function(
                    func, args=args, kwargs=kwargs, normalize_to_only_use_kwargs=True
                )
                # device of the input is returned
                return tree_map(partial(wrap, device=new_kwargs["input"].device), r)

            common_device = FakeTensor._find_common_device(func, args, kwargs)

            return tree_map(partial(wrap, device=common_device), r)

    def from_tensor(self, tensor):
        with no_dispatch():
            return self.fake_tensor_converter(self, tensor)
