from code_template import CodeTemplate
from function_wrapper import nested_dict

FILE = CodeTemplate("""\
#include "ATen/Config.h"

#include "TH/TH.h"
${cuda_includes}
#include "ATen/Utils.h"
${copy_includes}

namespace at {

${copy_functions}

}
""")

CUDA_INCLUDES = """\
#undef THNN_
#include "THC/THC.h"
"""

COPY = CodeTemplate("""\
${THTensor}_copy${cuda}${src_scalar_name}(${state,}\
static_cast<${dst_tensor}*>(dst.pImpl)->tensor, \
static_cast<${src_tensor}*>(src.pImpl)->tensor);
""")

COPY_ASYNC_CPU = CodeTemplate("""\
if (non_blocking) {
    ${THTensor}_copyAsyncCPU(${state,}\
static_cast<${dst_tensor}*>(dst.pImpl)->tensor, \
static_cast<${src_tensor}*>(src.pImpl)->tensor);
    break;
}
""")

COPY_ASYNC_CUDA = CodeTemplate("""\
if (non_blocking) {
    ${THTensor}_copyAsyncCuda(${state,}\
static_cast<${dst_tensor}*>(dst.pImpl)->tensor, \
static_cast<${src_tensor}*>(src.pImpl)->tensor);
    break;
}
""")

CASE = CodeTemplate("""\
case ${case_id}:
    ${copies}
    break;
""")

FUNCTION = CodeTemplate("""\
Tensor & ${Type}::s_copy_(Tensor & dst, const Tensor & src, bool non_blocking) const {
  // code generated by copy_wrapper
  checked_cast_tensor<${Tensor}>(dst.pImpl, "dst", 0, false);
  switch (src.type().ID()) {
    ${copy_body}
    default:
      ${function_fallthrough}
  }
  dst.pImpl->setScalar(src.pImpl->isScalar());
  return dst;
}
""")

FUNCTION_FALLTHROUGH_REDISPATCH = "return src.type().s_copy_from(src, dst, non_blocking);"

FUNCTION_FALLTHROUGH_ERROR = """\
AT_ERROR("copy does not support ", src.type().toString(), " to ", toString(), " copy.");
"""

FUNCTION_FROM = CodeTemplate("""\
Tensor & ${Type}::s_copy_from(const Tensor & src, Tensor & dst, bool non_blocking) const {
  // code generated by copy_wrapper
  checked_cast_tensor<${Tensor}>(src.pImpl, "src", 0, false);
  switch (dst.type().ID()) {
    ${copy_body}
    default:
      AT_ERROR("copy does not support ", toString(), " to ", dst.type().toString(), " copy.");
      break;
  }
  dst.pImpl->setScalar(src.pImpl->isScalar());
  return dst; // NB! dst
}
""")

# Technically, no code should actually call s_copy_from with a CPU self (this
# only can happen when the src is CUDA from a CPU kernel) but for
# completeness we fill out with a swap.
FUNCTION_FROM_SWAP = CodeTemplate("""\
Tensor & ${Type}::s_copy_from(const Tensor & src, Tensor & dst, bool non_blocking) const {
  return dst.type().s_copy_(dst, src, non_blocking);
}
""")


# all_types contains ONLY cpu types for a CPU
def create_one_copy(dst_type, all_types):
    copy_body = []

    for src_type in all_types:
        if dst_type['Density'] == 'Sparse' or src_type['Density'] == 'Sparse':
            # skip sparse copies, which are not yet implemented
            continue
        cuda = ''
        state = []
        if src_type['Backend'] == 'CUDA' or dst_type['Backend'] == 'CUDA':
            state.append('context->thc_state')
        if src_type['Backend'] == 'CUDA':
            if dst_type['Backend'] == 'CUDA':
                cuda = 'Cuda'
            else:
                # don't attempt to process CPU-CUDA; this is handled in the
                # redispatch
                continue

        body_env = nested_dict({
            'src_scalar_name': src_type['ScalarName'],
            'case_id': src_type['TypeID'],
            'src_tensor': src_type['Tensor'],
            'dst_tensor': dst_type['Tensor'],
            'cuda': cuda,
            'state': state,
        }, dst_type)

        copies = []
        if dst_type['ScalarType'] == src_type['ScalarType']:
            if dst_type['Backend'] == 'CUDA' and src_type['Backend'] == 'CPU':
                copies.append(COPY_ASYNC_CPU.substitute(body_env))
        copies.append(COPY.substitute(body_env))

        copy_body.append(CASE.substitute(body_env, copies=copies))

    if dst_type['Backend'] == 'CPU':
        # CPU fallthrough needs to redispatch to s_copy_from
        # (Backend == CPU implies Dense)
        assert dst_type['Density'] == 'Dense'
        function_fallthrough = FUNCTION_FALLTHROUGH_REDISPATCH
    else:
        function_fallthrough = FUNCTION_FALLTHROUGH_ERROR

    env = nested_dict({
        'function_fallthrough': function_fallthrough
    }, dst_type)
    return FUNCTION.substitute(env, copy_body=copy_body)


def create_one_copy_from(src_type, all_types):
    if src_type['DenseBackend'] == 'CPU':
        return FUNCTION_FROM_SWAP.substitute(src_type)

    copy_body = []

    for dst_type in all_types:
        if dst_type['Density'] == 'Sparse' or src_type['Density'] == 'Sparse':
            # skip sparse copies, which are not yet implemented
            continue
        cuda = ''
        state = []
        if src_type['Backend'] == 'CUDA':
            cuda = 'Cuda'
        if dst_type['Backend'] == 'CUDA' or src_type['Backend'] == 'CUDA':
            state.append('context->thc_state')

        body_env = nested_dict({
            'src_scalar_name': src_type['ScalarName'],
            'case_id': dst_type['TypeID'],
            'src_tensor': src_type['Tensor'],
            'dst_tensor': dst_type['Tensor'],
            'cuda': cuda,
            'state': state,
        }, dst_type)

        copies = []
        if dst_type['ScalarType'] == src_type['ScalarType']:
            if dst_type['Backend'] == 'CPU':
                copies.append(COPY_ASYNC_CUDA.substitute(body_env))
        copies.append(COPY.substitute(body_env))

        copy_body.append(CASE.substitute(body_env, copies=copies))

    return FUNCTION_FROM.substitute(src_type, copy_body=copy_body)


def create(all_types, backend):
    top_env = {
        'copy_includes': [],
        'copy_functions': [],
        'cuda_includes': [],
    }
    if backend == 'CPU':
        # NB: We are going to iterate over this multiple times; better not use a
        # generator
        # NB: Backend is 'CPU' or 'SparseCPU'
        all_types = list(filter((lambda t: t['DenseBackend'] == 'CPU'), all_types))
    else:
        top_env['cuda_includes'].append(CUDA_INCLUDES)
    for the_type in all_types:
        top_env['copy_includes'].append(
            '#include "ATen/{}.h"'.format(the_type['Type']))
        top_env['copy_includes'].append(
            '#include "ATen/{}.h"'.format(the_type['Tensor']))
        # Headers for everything in all_types, but generate code only for your
        # backend
        if the_type['DenseBackend'] != backend:
            continue
        top_env['copy_functions'].append(create_one_copy(the_type, all_types))
        top_env['copy_functions'].append(create_one_copy_from(the_type, all_types))
    return FILE.substitute(top_env)
