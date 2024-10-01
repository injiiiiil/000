# This file is run to generate the kernel instantiations for the flash_attn kernels
# They are written to several files in order to speed up compilation

import argparse
import itertools
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional


DTYPE_MAP = {
    "fp16": "cutlass::half_t",
    "bf16": "cutlass::bfloat16_t",
}

SM = [80]  # Sm80 kernels support up to
HEAD_DIMENSIONS = [32, 64, 96, 128, 160, 192, 256]
IS_CAUSAL = ["false", "true"]
KERNEL_IMPL_TEMPLATE_FWD = """
template<>
void run_mha_fwd_<{DTYPE}, {HEAD_DIM}, {IS_CAUSAL}>(Flash_fwd_params &params, cudaStream_t stream) {{
    run_mha_fwd_hdim{HEAD_DIM}<{DTYPE}, {IS_CAUSAL}>(params, stream);
}}
"""
KERNEL_IMPL_TEMPLATE_FWD_SPLIT = """

template void run_mha_fwd_splitkv_dispatch<{DTYPE}, {HEAD_DIM}, {IS_CAUSAL}>(Flash_fwd_params &params, cudaStream_t stream);
"""

KERNEL_IMPL_TEMPLATE_BWD = """
template<>
void run_mha_bwd_<{DTYPE}, {HEAD_DIM}, {IS_CAUSAL}>(Flash_bwd_params &params, cudaStream_t stream) {{
    run_mha_bwd_hdim{HEAD_DIM}<{DTYPE}, {IS_CAUSAL}>(params, stream);
}}
"""


@dataclass
class Kernel:
    sm: int
    dtype: str
    head_dim: int
    is_causal: bool
    direction: str

    @property
    def template(self) -> str:
        template_map = {
            "fwd": KERNEL_IMPL_TEMPLATE_FWD,
            "bwd": KERNEL_IMPL_TEMPLATE_BWD,
            "fwd_split": KERNEL_IMPL_TEMPLATE_FWD_SPLIT,  # Assuming 'split' for the default case
        }

        template = template_map[self.direction]

        return template.format(
            DTYPE=DTYPE_MAP[self.dtype],
            HEAD_DIM=self.head_dim,
            IS_CAUSAL=self.is_causal,
        )

    @property
    def filename(self) -> str:
        return (
            f"flash_{self.direction}_hdim{self.head_dim}_{self.dtype}_"
            f"{'causal_' if self.is_causal == 'true' else ''}"
            f"sm{self.sm}.cu"
        )


def get_all_kernels() -> List[Kernel]:
    for direction in ["fwd", "fwd_split", "bwd"]:
        for dtype, head_dim, is_causal, sm in itertools.product(
            DTYPE_MAP.keys(), HEAD_DIMENSIONS, IS_CAUSAL, SM
        ):
            yield Kernel(
                sm=sm,
                dtype=dtype,
                head_dim=head_dim,
                is_causal=is_causal,
                direction=direction,
            )


def write_kernel(kernel: Kernel, autogen_dir: Path) -> None:
    prelude = """
// Copyright (c) 2024, Tri Dao.

// Splitting the different head dimensions to different files to speed up compilation.
// This file is auto-generated. See "generate_kernels.py"\n
"""
    launch_template_str = kernel.direction if kernel.direction != "fwd_split" else "fwd"
    include = f"#include <ATen/native/transformers/cuda/flash_attn/flash_{launch_template_str}_launch_template.h>\n"
    namespace = "namespace pytorch_flash{\n"
    namespace_end = "} // namespace pytorch_flash\n"
    (autogen_dir / kernel.filename).write_text(
        prelude + include + namespace + kernel.template + namespace_end
    )


def main(output_dir: Optional[str]) -> None:
    if output_dir is None:
        output_dir = Path(__file__).parent
    else:
        output_dir = Path(output_dir)

    for kernel in get_all_kernels():
        write_kernel(kernel, output_dir)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="generate_kernels",
        description="Generate the flash_attention kernels template instantiations",
    )
    # Set an optional output directory
    parser.add_argument(
        "-o",
        "--output_dir",
        required=False,
        help="Where to generate the kernels " " will default to the current directory ",
    )
    args = parser.parse_args()
    main(args.output_dir)
