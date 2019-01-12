import torch
import torch.backends.cudnn as cudnn
from ..._jit_internal import weak_script


@weak_script
def affine_grid_generator(theta, size):
    # type: (Tensor, List[int]) -> Tensor
    if theta.is_cuda and cudnn.enabled and cudnn.is_acceptable(theta) and len(size) == 4:
        N, C, H, W = size
        ret = torch._C._nn.cudnn_affine_grid_generator(theta, N, C, H, W)
    else:
        ret = torch._C._nn.affine_grid_generator(theta, size)
    return ret
