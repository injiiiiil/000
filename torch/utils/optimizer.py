"""
This module contains utility method for mobile model optimization and lint.
"""

import io
import torch


def optimize_for_mobile(scripted_model):
    """
    Args:
        scripted_model: An instance of torch script module with type of ScriptModule

    Returns:
        scripted_model: And optimized torch script module.
    """
    if not isinstance(scripted_model, torch.jit.ScriptModule):
        raise TypeError(
            'Got {}, but ScriptModule is expected.'.format(type(scripted_model)))

    torch._C._jit_pass_optimize_for_mobile(scripted_model._c)
    buffer = io.BytesIO()
    torch.jit.save(scripted_model, buffer)
    buffer.seek(0)
    optimized_script_model = torch.jit.load(buffer)
    return optimized_script_model
