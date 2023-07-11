from datetime import timedelta
from typing import Any, Dict, List, Optional, overload, Tuple

import torch

from . import Future
from ._autograd import ProfilerEvent
from ._distributed_c10d import Store
from ._profiler import ProfilerConfig

# This module is defined in torch/csrc/distributed/rpc/init.cpp

_DEFAULT_INIT_METHOD: str
_DEFAULT_NUM_WORKER_THREADS: int
_UNSET_RPC_TIMEOUT: float
_DEFAULT_RPC_TIMEOUT_SEC: float

class RpcBackendOptions:
    rpc_timeout: float
    init_method: str
    def __init__(
        self,
        rpc_timeout: float = ...,
        init_method: str = ...,
    ): ...

class WorkerInfo:
    def __init__(self, name: str, worker_id: int): ...
    @property
    def name(self) -> str: ...
    @property
    def id(self) -> int: ...
    def __eq__(self, other: object) -> bool: ...

class RpcAgent:
    def join(self, shutdown: bool = False, timeout: float = 0): ...
    def sync(self): ...
    def shutdown(self): ...
    @overload
    def get_worker_info(self) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, workerName: str) -> WorkerInfo: ...
    def get_worker_infos(self) -> List[WorkerInfo]: ...
    def _get_device_map(self, dst: WorkerInfo) -> Dict[torch.device, torch.device]: ...
    def get_debug_info(self) -> Dict[str, str]: ...
    def get_metrics(self) -> Dict[str, str]: ...

class PyRRef:
    def __init__(self, value: Any, type_hint: Any = None): ...
    def is_owner(self) -> bool: ...
    def confirmed_by_owner(self) -> bool: ...
    def owner(self) -> WorkerInfo: ...
    def owner_name(self) -> str: ...
    def to_here(self, timeout: float = ...) -> Any: ...
    def local_value(self) -> Any: ...
    def rpc_sync(self, timeout: float = ...) -> Any: ...
    def rpc_async(self, timeout: float = ...) -> Any: ...
    def remote(self, timeout: float = ...) -> Any: ...
    def _serialize(self) -> Tuple: ...
    @staticmethod
    def _deserialize(tp: Tuple) -> PyRRef: ...
    def _get_type(self) -> Any: ...
    def _get_future(self) -> Future: ...
    def _get_profiling_future(self) -> Future: ...
    def _set_profiling_future(self, profilingFuture: Future): ...

class _TensorPipeRpcBackendOptionsBase(RpcBackendOptions):
    num_worker_threads: int
    device_maps: Dict[str, Dict[torch.device, torch.device]]
    devices: List[torch.device]
    def __init__(
        self,
        num_worker_threads: int,
        _transports: Optional[List],
        _channels: Optional[List],
        rpc_timeout: float = ...,
        init_method: str = ...,
        device_maps: Dict[str, Dict[torch.device, torch.device]] = {},
        devices: List[torch.device] = [],
    ): ...
    def _set_device_map(
        self,
        to: str,
        device_map: Dict[torch.device, torch.device],
    ): ...

class TensorPipeAgent(RpcAgent):
    def __init__(
        self,
        store: Store,
        name: str,
        worker_id: int,
        world_size: Optional[int],
        opts: _TensorPipeRpcBackendOptionsBase,
        reverse_device_maps: Dict[str, Dict[torch.device, torch.device]],
        devices: List[torch.device],
    ): ...
    def join(self, shutdown: bool = False, timeout: float = 0): ...
    def shutdown(self): ...
    @overload
    def get_worker_info(self) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, workerName: str) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, id: int) -> WorkerInfo: ...
    def get_worker_infos(self) -> List[WorkerInfo]: ...
    def _get_device_map(self, dst: WorkerInfo) -> Dict[torch.device, torch.device]: ...
    def _update_group_membership(
        self,
        worker_info: WorkerInfo,
        my_devices: List[torch.device],
        reverse_device_map: Dict[str, Dict[torch.device, torch.device]],
        is_join: bool,
    ): ...
    def _get_backend_options(self) -> _TensorPipeRpcBackendOptionsBase: ...
    @property
    def is_static_group(self) -> bool: ...
    @property
    def store(self) -> Store: ...

def _is_current_rpc_agent_set() -> bool: ...
def _get_current_rpc_agent() -> RpcAgent: ...
def _set_and_start_rpc_agent(agent: RpcAgent): ...
def _reset_current_rpc_agent(): ...
def _delete_all_user_and_unforked_owner_rrefs(timeout: timedelta = ...): ...
def _destroy_rref_context(ignoreRRefLeak: bool): ...
def _rref_context_get_debug_info() -> Dict[str, str]: ...
def _cleanup_python_rpc_handler(): ...
def _invoke_rpc_builtin(
    dst: WorkerInfo,
    opName: str,
    rpcTimeoutSeconds: float,
    *args: Any,
    **kwargs: Any,
): ...
def _invoke_rpc_python_udf(
    dst: WorkerInfo,
    pickledPythonUDF: str,
    tensors: List[torch.Tensor],
    rpcTimeoutSeconds: float,
    isAsyncExecution: bool,
): ...
def _invoke_rpc_torchscript(
    dstWorkerName: str,
    qualifiedNameStr: str,
    argsTuple: Tuple,
    kwargsDict: Dict,
    rpcTimeoutSeconds: float,
    isAsyncExecution: bool,
): ...
def _invoke_remote_builtin(
    dst: WorkerInfo,
    opName: str,
    rpcTimeoutSeconds: float,
    *args: Any,
    **kwargs: Any,
): ...
def _invoke_remote_python_udf(
    dst: WorkerInfo,
    pickledPythonUDF: str,
    tensors: List[torch.Tensor],
    rpcTimeoutSeconds: float,
    isAsyncExecution: bool,
): ...
def _invoke_remote_torchscript(
    dstWorkerName: WorkerInfo,
    qualifiedNameStr: str,
    rpcTimeoutSeconds: float,
    isAsyncExecution: bool,
    *args: Any,
    **kwargs: Any,
): ...
def get_rpc_timeout() -> float: ...
def enable_gil_profiling(flag: bool): ...
def _set_rpc_timeout(rpcTimeoutSeconds: float): ...

class RemoteProfilerManager:
    @staticmethod
    def set_current_profiling_key(key: str): ...

def _enable_server_process_global_profiler(new_config: ProfilerConfig): ...
def _disable_server_process_global_profiler() -> List[List[List[ProfilerEvent]]]: ...
def _set_profiler_node_id(default_node_id: int): ...
def _enable_jit_rref_pickle(): ...
def _disable_jit_rref_pickle(): ...
