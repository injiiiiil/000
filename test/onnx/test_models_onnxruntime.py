from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

import unittest
import onnxruntime  # noqa

from test_models import TestModels
from test_pytorch_onnx_onnxruntime import run_model_test
import torch


def exportTest(self, model, inputs, rtol=1e-2, atol=1e-7, opset_versions=None):
    opset_versions = opset_versions if opset_versions else [7, 8, 9, 10, 11, 12]
    is_script_test_enabled = True

    for opset_version in opset_versions:
        self.opset_version = opset_version
        run_model_test(self, model, False,
                       input=inputs, rtol=rtol, atol=atol)

        if is_script_test_enabled and opset_version > 11:
            outputs = model(inputs)
            script_model = torch.jit.script(model)
            run_model_test(self, script_model, False, example_outputs=outputs,
                           input=inputs, rtol=rtol, atol=atol)


if __name__ == '__main__':
    TestModels.exportTest = exportTest
    unittest.main()
