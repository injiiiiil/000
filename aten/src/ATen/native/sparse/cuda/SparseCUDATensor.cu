#include <ATen/ATen.h>
#include <ATen/cuda/CUDAContext.h>
#include <ATen/NativeFunctions.h>
#include <ATen/SparseTensorUtils.h>
#include <ATen/native/sparse/cuda/SparseCUDAApplyUtils.cuh>
#include <ATen/AccumulateType.h>
#include <ATen/cuda/CUDAApplyUtils.cuh>

#include <THC/THCThrustAllocator.cuh>
#include <THC/THCTensorSort.cuh>

#include <thrust/device_ptr.h>
#include <thrust/device_vector.h>
#include <thrust/gather.h>
#include <thrust/generate.h>
#include <thrust/scan.h>
#include <thrust/sequence.h>
#include <thrust/sort.h>
#include <thrust/transform.h>
#include <thrust/unique.h>
#include <thrust/system/cuda/execution_policy.h>
#include <c10/macros/Macros.h>

namespace at { namespace native {

using namespace at::sparse;

SparseTensor coalesce_sparse_cuda(const SparseTensor& self) {
  if (self.is_coalesced()) {
    return self;
  }
  int64_t nse = self.nse(false);
  // NOTE: Since `coalesce` is not an in-place operation when `is_coalesced` is false,
  // we should keep the original tensor intact and do coalesce on a copy of the tensor
  if (nse < 2) {
    SparseTensor dst = self.clone();
    dst._coalesced_(true);
    return dst;
  }

  cudaStream_t stream = at::cuda::getCurrentCUDAStream();
  auto allocator = THCThrustAllocator(globalContext().lazyInitCUDA());
  auto policy = thrust::cuda::par(allocator).on(stream);
  // Replace instances with

  // For indices, a simple sort + unique suffices
  // For values, we use a custom kernel for segmented reduction (can't use Thrust due to indirection).

  Tensor values = self.values(false);

  int64_t sparse_dim = self.sparse_dim();

  // indices will be modified by Thrust, so we have to clone or use new storage
  // here.
  LongTensor indices1D = flatten_indices(self.indices(false), self.sizes(), true);

  LongTensor origIndices = at::empty({nse}, self.indices(false).options());
  LongTensor uniqueOffsets = at::empty({nse}, self.indices(false).options());

  typedef thrust::device_ptr<int64_t> thrust_ptr;
  thrust_ptr indicesIter(indices1D.data_ptr<int64_t>());
  thrust_ptr origIndicesIter(origIndices.data_ptr<int64_t>());
  thrust_ptr uniqueOffsetsIter(uniqueOffsets.data_ptr<int64_t>());


  // Fill sortedOrigIndices with sequential indices
  thrust::counting_iterator<int64_t> countIterI(0);
  thrust::counting_iterator<int64_t> countIterO(0);

  thrust::copy(policy, countIterI, countIterI + nse, origIndicesIter);
  thrust::copy(policy, countIterO, countIterO + nse, uniqueOffsetsIter);

  thrust::sort_by_key(policy,
    indicesIter, indicesIter + nse,
    origIndicesIter, ThrustLTOp<int64_t>()
  );

  // this forces device-host synchronization!
  thrust::pair<thrust_ptr, thrust_ptr> newEnd = thrust::unique_by_key(policy,
    indicesIter, indicesIter + nse,
    uniqueOffsetsIter
  );
  int64_t newNse = newEnd.first - indicesIter;

  indices1D.resize_({1, newNse});
  auto newValues_size = values.sizes().vec();
  newValues_size[0] = newNse;
  Tensor newValues = at::empty(newValues_size, values.options());

  // If there is no values to copy, save running the kernel.
  if (newValues.numel() > 0) {
    const int SZ = 4;
    values = values.contiguous();
    int64_t stride = at::prod_intlist(values.sizes().slice(1));
    dim3 grid(THCCeilDiv(newNse, (int64_t) SZ), THCCeilDiv(stride, (int64_t) C10_WARP_SIZE*SZ));
    dim3 block(C10_WARP_SIZE, SZ);
    AT_DISPATCH_ALL_TYPES_AND2(
      at::ScalarType::Half, at::ScalarType::BFloat16, values.scalar_type(), "coalesce_sparse_cuda", [&] {
        AT_SKIP_BFLOAT16_IF_NOT_ROCM(scalar_t, "coalesce_sparse_cuda", [&] {
          using cuda_accscalar_t = acc_type<scalar_t, /* is_cuda */ true>;
          apply::coalesceValuesKernel<scalar_t, cuda_accscalar_t><<<grid, block, 0, stream>>>(
            uniqueOffsets.data_ptr<int64_t>(),
            origIndices.data_ptr<int64_t>(),
            values.data_ptr<scalar_t>(),
            newValues.data_ptr<scalar_t>(),
            nse,
            newNse,
            stride
          );
        });
      });
  }

// this grid-strided version is slower but probably more flexible
  // to different sizes
  // int64_t blockX = min(stride, (int64_t) 512);
  // dim3 block(blockX, 512 / blockX);
  // int64_t grid = min((int64_t) 1024, THCCeilDiv((int64_t) newNse * stride, (int64_t) block.x * block.y));
  // THCSTensor_coalesceValuesKernel_gridStrided<real, accreal><<<grid, block, 0, stream>>>(
  //   THCIndexTensor_(data)(state, uniqueOffsets),
  //   THCIndexTensor_(data)(state, origIndices),
  //   THCTensor_(data)(state, values),
  //   THCTensor_(data)(state, newValues),
  //   nse,
  //   newNse,
  //   stride
  // );

  ////////////////////////////////////////////////////////////
  // unflatten indices if necessary
  LongTensor newIndices;
  if (sparse_dim == 1) {
    newIndices = indices1D;
  } else {
    newIndices = at::empty({sparse_dim, newNse}, origIndices.options());
    for (int64_t d = sparse_dim - 1; d >= 0; d--) {
      // NB: Not a select, so I can preserve the outer dimension
      LongTensor indicesSlice = newIndices.narrow(0, d, 1);
      // Note for the porting guide: THCTensor_(copy) does NOT do normal
      // broadcasting logic; instead, it will blast the elements from one
      // to the other so long as the numel is the same
      indicesSlice.copy_(indices1D);
      indices1D.floor_divide_(self.size(d));
      indicesSlice.add_(indices1D, -self.size(d));
    }
  }
  ////////////////////////////////////////////////////////////
  // We can use unsafe sparse tensor constructor because the indices do not
  // need to be revalidated as we do not add or change indices, just remove
  // duplicates.
  SparseTensor dst = ::at::native::_sparse_coo_tensor_unsafe(newIndices, newValues, self.sizes())._coalesced_(true);

  THCudaCheck(cudaGetLastError());
  return dst;
}

}} // namespace at::native
