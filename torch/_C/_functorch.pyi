from torch import Tensor
from enum import Enum
from typing import Optional, Tuple

# Defined in torch/csrc/functorch/init.cpp

def _set_dynamic_layer_keys_included(included: bool) -> None: ...
def get_unwrapped(tensor: Tensor) -> Tensor: ...
def is_batchedtensor(tensor: Tensor) -> bool: ...
def is_functionaltensor(tensor: Tensor) -> bool: ...
def is_functorch_wrapped_tensor(tensor: Tensor) -> bool: ...
def is_gradtrackingtensor(tensor: Tensor) -> bool: ...
def maybe_get_bdim(tensor: Tensor) -> int: ...
def maybe_get_level(tensor: Tensor) -> int: ...
def unwrap_if_dead(tensor: Tensor) -> Tensor: ...
def _unwrap_for_grad(tensor: Tensor, level: int) -> Tensor: ...
def _wrap_for_grad(tensor: Tensor, level: int) -> Tensor: ...
def _unwrap_batched(tensor: Tensor, level: int) -> Tuple[Tensor, Optional[int]]: ...
def current_level() -> int: ...
def _add_batch_dim(tensor: Tensor, bdim: int, level: int) -> Tensor: ...

def set_autograd_function_allowed(allowed: bool) -> None: ...
def get_autograd_function_allowed() -> bool: ...

# Defined in aten/src/ATen/functorch/Interpreter.h
class TransformType(Enum):
    Torch: TransformType = ...
    Vmap: TransformType = ...
    Grad: TransformType = ...
    Jvp: TransformType = ...
    Functionalize: TransformType = ...

class RandomnessType(Enum):
    Error: TransformType = ...
    Same: TransformType = ...
    Different: TransformType = ...

class CInterpreter:
    def key(self) -> TransformType: ...
    def level(self) -> int: ...

class CGradInterpreterPtr:
    def __init__(self, interpreter: CInterpreter): ...
    def lift(self, Tensor) -> Tensor: ...
    def prevGradMode(self) -> bool: ...

class CJvpInterpreterPtr:
    def __init__(self, interpreter: CInterpreter): ...
    def lift(self, Tensor) -> Tensor: ...
    def prevFwdGradMode(self) -> bool: ...

class CFunctionalizeInterpreterPtr:
    def __init__(self, interpreter: CInterpreter): ...
    def key(self) -> TransformType: ...
    def level(self) -> int: ...
    def functionalizeAddBackViews(self) -> bool: ...

class CVmapInterpreterPtr:
    def __init__(self, interpreter: CInterpreter): ...
    def key(self) -> TransformType: ...
    def level(self) -> int: ...
    def batchSize(self) -> int: ...
    def randomness(self) -> RandomnessType: ...

class DynamicLayer:
    pass

def peek_interpreter_stack() -> CInterpreter: ...
def pop_dynamic_layer_stack() -> DynamicLayer: ...
def push_dynamic_layer_stack(dl: DynamicLayer) -> int: ...
