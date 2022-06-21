r"""
This file contains utility functions to convert values
using APoT nonuniform quantization methods.
"""

import math

r"""Converts floating point input into int4 APoT2 number
    based on quantization levels
"""
def float_to_apot(x, levels, indices):
    levels_lst = list(levels)
    indices_lst = list(indices)

    min_delta = math.inf
    best_idx = 0

    for level, idx in zip(levels_lst, indices_lst):
        cur_delta = abs(level - x)
        if cur_delta < min_delta:
            min_delta = cur_delta
            best_idx = idx

    return best_idx

r"""Converts floating point input into
    reduced precision floating point value
    based on quantization levels
"""
def float_to_reduced_precision(x, levels, indices):
    levels_lst = list(levels)
    indices_lst = list(indices)

    min_delta = math.inf
    best_fp = 0.0

    for level, idx in zip(levels_lst, indices_lst):
        cur_delta = abs(level - x)
        if cur_delta < min_delta:
            min_delta = cur_delta
            best_fp = level

    return best_fp

r"""Converts int4 APoT2 input into floating point number
based on quantization levels
"""
def apot_to_float(x_apot, levels, indices):
    idx = list(indices).index(x_apot)
    return levels[idx]
