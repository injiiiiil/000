from __future__ import absolute_import, division, print_function, unicode_literals
from ...modules.linear import Linear as NNLinear
from torch.quantization.QConfig import default_qat_qconfig
import torch.nn.functional as F

class Linear(NNLinear):
    r"""
    A linear module attached with FakeQuantize modules for both output
    activation and weight, used for quantization aware training.

    We adopt the same interface as `torch.nn.Linear`, please see
    https://pytorch.org/docs/stable/nn.html#torch.nn.Linear
    for documentation.

    Similar to `torch.nn.Linear`, with FakeQuantize modules initialized to
    default.

    Attributes:
        observer: fake quant module for output activation, it's called observer
            to align with post training flow
        weight: fake quant module for weight
    """
    __FLOAT_MODULE = NNLinear

    def __init__(self, in_features, out_features, bias=True,
                 qconfig=None):
        super(Linear, self).__init__(in_features, out_features, bias)
        self.qconfig = qconfig
        assert qconfig, 'qconfig must be provided for QAT module'
        self.observer = qconfig.activation()
        self.weight_fake_quant = qconfig.weight()

    def forward(self, input):
        return self.observer(F.linear(input, self.weight_fake_quant(self.weight), self.bias))

    @classmethod
    def from_float(cls, mod, qconfig=None):
        r"""Create a qat module from a float module or qparams_dict

            Args: `mod` a float module, either produced by torch.quantization utilities
            or directly from user
        """
        assert type(mod) == cls.__FLOAT_MODULE, ' qat.' + cls.__name__ + '.from_float only works for ' + \
            cls.__FLOAT_MODULE.__name__
        if not qconfig:
            assert hasattr(mod, 'qconfig'), 'Input float module must have qconfig defined'
            assert mod.qconfig, 'Input float module must has valid qconfig'
            qconfig = mod.qconfig
        qat_linear = cls(mod.in_features, mod.out_features, bias=mod.bias is not None, qconfig=qconfig)
        qat_linear.weight = mod.weight
        qat_linear.bias = mod.bias
        return qat_linear
