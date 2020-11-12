from typing import Tuple, Dict, Optional, List, Any, overload
from datetime import timedelta
import enum
import torch
from . import Future
from ._autograd import ProfilerConfig, ProfilerState, ProfilerEvent
from ._distributed_c10d import ProcessGroup, Store

# This module is defined in torch/csrc/distributed/rpc/init.cpp

_DEFAULT_NUM_SEND_RECV_THREADS: int
_DEFAULT_INIT_METHOD: str
_DEFAULT_NUM_WORKER_THREADS: int
_UNSET_RPC_TIMEOUT: float
_DEFAULT_RPC_TIMEOUT_SEC: float

class RpcBackendOptions:
    rpc_timeout: float
    init_method: str
    def __init__(
        self,
        rpc_timeout: float = _DEFAULT_RPC_TIMEOUT_SEC,
        init_method: str = _DEFAULT_INIT_METHOD,
    ): ...

class WorkerInfo:
    def __init__(self, name: str, worker_id: int): ...
    @property
    def name(self) -> str: ...
    @property
    def id(self) -> int: ...
    def __eq__(self, other: object) -> bool: ...
    def __repr__(self) -> str: ...

class RpcAgent:
    def join(self): ...
    def sync(self): ...
    def shutdown(self): ...
    @overload
    def get_worker_info(self) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, workerName: str) -> WorkerInfo: ...
    def get_worker_infos(self) -> List[WorkerInfo]: ...
    def get_debug_info(self) -> Dict[str, str]: ...
    def get_metrics(self) -> Dict[str, str]: ...

class PyRRef:
    def __init__(self, value: Any, type_hint: Any = None): ...
    def is_owner(self) -> bool: ...
    def confirmed_by_owner(self) -> bool: ...
    def owner(self) -> WorkerInfo: ...
    def owner_name(self) -> str: ...
    def to_here(self, timeout: float = _UNSET_RPC_TIMEOUT) -> Any: ...
    def local_value(self) -> Any: ...
    def rpc_sync(self) -> Any: ...
    def rpc_async(self) -> Any: ...
    def remote(self) -> Any: ...
    def _serialize(self) -> Tuple: ...
    @staticmethod
    def _deserialize(tp: Tuple) -> 'PyRRef': ...
    def _get_type(self) -> Any: ...
    def _get_future(self) -> Future: ...
    def _get_profiling_future(self) -> Future: ...
    def _set_profiling_future(self, profilingFuture: Future): ...
    def __repr__(self) -> str: ...
    ...

class ProcessGroupRpcBackendOptions(RpcBackendOptions):
    num_send_recv_threads: int
    def __init__(
        self,
        num_send_recv_threads: int,
        rpc_timeout: float,
        init_method: str
    ): ...

class ProcessGroupAgent(RpcAgent):
    def __init__(
        self,
        worker_name: str,
        pg: ProcessGroup,
        numSendRecvThreads: int,
        rpcTimeout: timedelta
    ): ...
    @overload
    def get_worker_info(self) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, workerName: str) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, id: int) -> WorkerInfo: ...
    def get_worker_infos(self) -> List[WorkerInfo]: ...
    def join(self): ...
    def shutdown(self): ...
    def sync(self): ...

class _TensorPipeRpcBackendOptionsBase(RpcBackendOptions):
    num_worker_threads: int
    device_maps: Dict[str, Dict[int, int]]
    def __init__(
        self,
        num_worker_threads: int,
        _transports: Optional[List],
        _channels: Optional[List],
        rpc_timeout: float = _DEFAULT_RPC_TIMEOUT_SEC,
        init_method: str = _DEFAULT_INIT_METHOD,
        device_maps: Dict[str, Dict[int, int]] = dict()): ...
    def set_device_map(self, to: str, device_map: Dict[str, Dict[int, int]]): ...

class TensorPipeAgent(RpcAgent):
    def __init__(
        self,
        store: Store,
        name: str,
        worker_id: int,
        world_size: int,
        pg: ProcessGroup,
        opts: _TensorPipeRpcBackendOptionsBase,
    ): ...
    def join(self): ...
    def shutdown(self): ...
    @overload
    def get_worker_info(self) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, workerName: str) -> WorkerInfo: ...
    @overload
    def get_worker_info(self, id: int) -> WorkerInfo: ...
    def get_worker_infos(self) -> List[WorkerInfo]: ...
    def _set_reverse_device_maps(self, reverseDeviceMaps: Dict[str, Dict[int, int]]): ...

def _is_current_rpc_agent_set() -> bool: ...
def _get_current_rpc_agent()-> RpcAgent: ...
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
    **kwargs: Any
    ): ...
def _invoke_rpc_python_udf(
    dst: WorkerInfo,
    pickledPythonUDF: str,
    tensors: List[torch.Tensor],
    rpcTimeoutSeconds: float,
    isAsyncExecution: bool
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
    **kwargs: Any
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
    **kwargs: Any
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
