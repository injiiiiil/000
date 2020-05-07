from typing import Callable, Optional, List
from torch.cuda.amp import GradScaler
from .adamw import AdamW


class SRAdamW(AdamW):
    def step(self, closure: Optional[Callable[[], float]]=..., grad_scaler: GradScaler=...) -> Optional[float]: ...
