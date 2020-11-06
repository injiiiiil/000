#include <ATen/CUDAGeneratorImpl.h>
#include <ATen/cuda/CUDAContext.h>
#include <ATen/cuda/nvrtc_stub/ATenNVRTC.h>

#include <c10/cuda/CUDACachingAllocator.h>

#include <torch/csrc/jit/codegen/cuda/executor_utils.h>
#include <torch/csrc/jit/codegen/cuda/instrumentation.h>
#include <torch/csrc/jit/codegen/cuda/ir_all_nodes.h>
#include <torch/csrc/jit/codegen/cuda/kernel_ir_printer.h>
#include <torch/csrc/jit/resource_guard.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <cstring>
#include <fstream>
#include <string>

namespace torch {
namespace jit {
namespace fuser {
namespace cuda {
namespace executor_utils {

std::string kernelPreamble() {
  std::stringstream ss;

#ifndef __HIP_PLATFORM_HCC__
  ss << R"(
    #include <nvfuser_runtime/fp16_support.cu>
  )";
#endif

  ss << R"(
    #include <nvfuser_runtime/tensor.cu>
    #include <nvfuser_runtime/random_numbers.cu>
    #include <nvfuser_runtime/helpers.cu>
    #include <nvfuser_runtime/block_reduction.cu>
    #include <nvfuser_runtime/grid_reduction.cu>
    #include <nvfuser_runtime/broadcast.cu>
  )";

  return ss.str();
}

namespace {

std::string torchLibPath() {
#ifdef _WIN32
// TODO $$$
#else
  // Used to lookup our library using dladdr()
  static bool reference_symbol = false;

  Dl_info dl_info = {};
  TORCH_INTERNAL_ASSERT(dladdr(&reference_symbol, &dl_info));
  const char* lib_filename = dl_info.dli_fname;

  // TODO: a much better solution would be to use C++17's std::filesystem::path,
  //  although that's currently not available to us (in the PyTorch codebase)
  const char* last_path_sep = std::strrchr(lib_filename, '/');
  TORCH_INTERNAL_ASSERT(last_path_sep != nullptr);

  return std::string(lib_filename, last_path_sep);
#endif
}

// return false if arg's type, number of dimensions, and device, doesn't match
// param and provided c10:device
bool validateKernelArgTensor(
    const at::Tensor& arg,
    const Val* param,
    const c10::Device& device,
    std::stringstream& msg) {
  // Arg is a tensor. Param must be a tensor too.
  if (*param->getValType() != ValType::TensorView) {
    msg << "Argument is a tensor, but the parameter is not.\n";
    return false;
  }

  // Check the rank of the tensors.
  size_t arg_dim = arg.dim();
  // Note: This requires current Fusion to be active.
  size_t param_dim =
      TensorDomain::noReductions(param->as<TensorView>()->getRootDomain())
          .size();
  // see [Note - broadcast support in integration]
  // Because of broadcasting support handled in integration, we relax the rank
  // check as necessary.
  if (arg_dim > param_dim) {
    msg << "Argument tensor's rank is " << arg_dim << ", but the parameter is "
        << param_dim << "\n";
    return false;
  }

  if (arg.device() != device) {
    msg << "Argument is on device that is not compiled for."
        << "\n";
    return false;
  }
  // Check element type
  at::ScalarType arg_data_type = arg.scalar_type();
  DataType param_data_type = *param->getDataType();
  bool match = false;
  switch (arg_data_type) {
    case at::ScalarType::Half:
      match = param_data_type == DataType::Half;
      break;
    case at::ScalarType::Float:
      match = param_data_type == DataType::Float;
      break;
    case at::ScalarType::Bool:
      match = param_data_type == DataType::Bool;
      break;
    default:
      msg << "Argument element type, " << arg_data_type << ", is not supported."
          << "\n";
      return false;
  }
  if (!match)
    msg << "Argument element type is " << arg_data_type
        << ", but the parameter is " << param_data_type << "\n";
  return match;
}

// Return false if  arg_type doesn't match the type in param
bool validateKernelArgScalar(
    const c10::TypePtr& arg_type,
    const Val* param,
    std::stringstream& msg) {
  if (!param->isScalar()) {
    msg << "Argument is a scalar, but the parameter is not."
        << "\n";
    return false;
  }
  DataType param_type = *param->getDataType();
  bool match = false;
  switch (arg_type->kind()) {
    case c10::TypeKind::IntType:
      match = param_type == DataType::Int;
      break;
    case c10::TypeKind::FloatType:
      match = param_type == DataType::Float;
      break;
    case c10::TypeKind::BoolType:
      match = param_type == DataType::Bool;
      break;
    default:
      match = false;
  }
  if (!match) {
    msg << "Argument type is " << *arg_type << ", but the parameter is "
        << param_type << "\n";
  }
  return match;
}

// Return false if arg and param don't match up and if arg's device (if a
// tensor) doesn't match provided device
bool validateKernelArg(
    const c10::IValue& arg,
    const Val* param,
    const c10::Device& device,
    std::stringstream& msg) {
  if (arg.isTensor()) {
    return validateKernelArgTensor(arg.toTensor(), param, device, msg);
  } else {
    return validateKernelArgScalar(arg.type(), param, msg);
  }
}

} // namespace

void validateKernelInputs(
    Fusion* fusion,
    const at::ArrayRef<IValue>& inputs,
    const c10::Device& device) {
  FUSER_PERF_SCOPE("validateKernelInputs");

  // This is necessary as we were traversing the fusion graph later in the check
  FusionGuard fg(fusion);
  // Check inputs
  TORCH_INTERNAL_ASSERT(
      inputs.size() == fusion->inputs().size(),
      "Wrong number of kernel inputs.");

  std::stringstream msg;
  bool mismatch = false;
  for (size_t i = 0; i < inputs.size(); ++i) {
    const IValue& arg = inputs[i];
    const Val* param = fusion->inputs()[i];
    mismatch = !validateKernelArg(arg, param, device, msg) || mismatch;
  }
  TORCH_INTERNAL_ASSERT(
      !mismatch, "Found one or more invalid arguments: ", msg.str());
}

void validateKernelOutputs(
    Fusion* fusion,
    const std::vector<at::Tensor>& outputs,
    const c10::Device& device) {
  FUSER_PERF_SCOPE("validateKernelOutputs");

  TORCH_INTERNAL_ASSERT(
      fusion->outputs().size() != 0,
      "Kernel should have at least one output tensor.");

  TORCH_INTERNAL_ASSERT(
      outputs.size() == fusion->outputs().size(),
      "Wrong number of kernel outputs.");

  std::stringstream msg;
  bool mismatch = false;
  for (size_t i = 0; i < outputs.size(); ++i) {
    const at::Tensor& arg = outputs[i];
    const Val* param = fusion->outputs()[i];
    mismatch = !validateKernelArg(arg, param, device, msg) || mismatch;
  }
  TORCH_INTERNAL_ASSERT(
      !mismatch, "Found one or more invalid arguments: ", msg.str());
}

kir::ExpressionEvaluator bindKernelInputs(
    const at::ArrayRef<IValue>& aten_inputs,
    kir::Kernel* kernel) {
  FUSER_PERF_SCOPE("bindKernelInputs");

  TORCH_INTERNAL_ASSERT(
      kernel->inputs().size() == aten_inputs.size(),
      "Something went wrong configuring launch. Inputs no longer match.");

  kir::ExpressionEvaluator expr_eval;
  const auto& inputs = kernel->inputs();

  for (size_t i = 0; i < inputs.size(); i++) {
    const auto input = inputs[i];

    if (auto tensor_input = dynamic_cast<kir::TensorView*>(input)) {
      TORCH_INTERNAL_ASSERT(
          aten_inputs[i].isTensor(),
          "Something went wrong configuring launch. Inputs no longer match.");

      const auto aten_tensor = aten_inputs[i].toTensor();
      const auto root_domain =
          kir::TensorDomain::noReductions(tensor_input->domain()->rootDomain());
      TORCH_INTERNAL_ASSERT(
          aten_tensor.ndimension() == static_cast<int>(root_domain.size()),
          "Something went wrong configuring launch. Inputs no longer match.");

      for (size_t dim = 0; dim < root_domain.size(); dim++) {
        const auto extent = root_domain[dim]->extent();
        const auto value = aten_tensor.sizes()[dim];
        const auto prev_value = expr_eval.evaluate(extent);
        if (prev_value.has_value()) {
          TORCH_CHECK(
              *prev_value == value,
              "Attempting to bind ",
              kir::toString(extent),
              " to ",
              value,
              "but it's already set to ",
              *prev_value);
        } else {
          expr_eval.bind(extent, value);
        }
      }
    } else if (input->isScalar() && input->dtype() == DataType::Int) {
      TORCH_INTERNAL_ASSERT(
          aten_inputs[i].type()->kind() == c10::TypeKind::IntType);
      expr_eval.bind(input, aten_inputs[i].toInt());
    }
  }

  return expr_eval;
}

ExpressionEvaluator bindFusionInputs(
    const at::ArrayRef<IValue>& aten_inputs,
    Fusion* fusion) {
  FUSER_PERF_SCOPE("bindFusionInputs");

  TORCH_INTERNAL_ASSERT(
      fusion->inputs().size() == aten_inputs.size(),
      "Something went wrong configuring launch. Inputs no longer match.");

  ExpressionEvaluator evaluator(fusion);
  auto inputs = fusion->inputs();

  // This should probably move to EvaluationContext as we may want to bind
  // input values frequently. Bind fusion input values to runtime values.
  for (size_t i = 0; i < inputs.size(); i++) {
    if (inputs[i]->getValType() == ValType::TensorView) {
      TensorView* cg_tensor = inputs[i]->as<TensorView>();

      TORCH_INTERNAL_ASSERT(
          aten_inputs[i].isTensor(),
          "Something went wrong configuring launch. Inputs no longer match.");

      auto aten_tensor = aten_inputs[i].toTensor();
      auto root_dom = TensorDomain::noReductions(cg_tensor->getRootDomain());
      TORCH_INTERNAL_ASSERT(
          aten_tensor.ndimension() == (int64_t)root_dom.size(),
          "Something went wrong configuring launch. Inputs no longer match.");

      for (size_t dim = 0; dim < root_dom.size(); dim++) {
        const auto extent = root_dom[dim]->extent();
        const auto value = aten_tensor.sizes()[dim];
        const auto prev_value = evaluator.evaluate(extent);
        if (prev_value.has_value()) {
          TORCH_CHECK(
              *prev_value == value,
              "Attempting to bind ",
              extent,
              " to ",
              value,
              "but it's already set to ",
              *prev_value);
        } else {
          evaluator.bind(extent, value);
        }
      }
    } else if (
        inputs[i]->getValType().value() == ValType::Scalar &&
        inputs[i]->getDataType().value() == DataType::Int) {
      TORCH_INTERNAL_ASSERT(
          aten_inputs[i].type()->kind() == c10::TypeKind::IntType);
      evaluator.bind(inputs[i], aten_inputs[i].toInt());
    }
  }
  return evaluator;
}

NvrtcFunction nvrtcCompile(
    const std::string& code,
    const std::string& func_name,
    int id) {
  FUSER_PERF_SCOPE("NVRTC");

  // lazily construct context if non-existing yet;
  CUcontext pctx = nullptr;
  AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuCtxGetCurrent(&pctx));
  if (!pctx) {
    std::unique_lock<std::mutex> cudaFreeMutexLock(
        *(c10::cuda::CUDACachingAllocator::getFreeMutex()));
    cudaFree(nullptr);
  }

  const auto prop = at::cuda::getCurrentDeviceProperties();
  int nvrtc_major, nvrtc_minor;
  AT_CUDA_NVRTC_CHECK(
      at::globalContext().getNVRTC().nvrtcVersion(&nvrtc_major, &nvrtc_minor));

  // Short-circuits if NVRTC version too low
  TORCH_INTERNAL_ASSERT(nvrtc_major >= 6);
  // Major and minor is determined by device properties and
  // possibly "downcompiled" to a lower (compatible) compute architecture
  // based on the NVRTC version
  const int major = prop->major;
  const int minor = prop->minor;
  nvrtcProgram program;

  {
    FUSER_PERF_SCOPE("nvrtcCreateProgram");
    AT_CUDA_NVRTC_CHECK(at::globalContext().getNVRTC().nvrtcCreateProgram(
        &program, code.c_str(), nullptr, 0, nullptr, nullptr));
  }

  ResourceGuard holdProgram([&] {
    FUSER_PERF_SCOPE("nvrtcDestroyProgram");
    AT_CUDA_NVRTC_CHECK(
        at::globalContext().getNVRTC().nvrtcDestroyProgram(&program));
  });

#ifdef __HIP_PLATFORM_HCC__
  std::vector<const char*> args = {"--std=c++14"};
#else
  const std::string compute = "--gpu-architecture=compute_" +
      std::to_string(major) + std::to_string(minor);
  std::vector<const char*> args = {
      "--std=c++14", compute.c_str(), "-default-device"};
#endif

  const char* disable_fma = getenv("PYTORCH_CUDA_FUSER_DISABLE_FMA");
  // int disable_fma_flag = disable_fma ? atoi(disable_fma) : 0;
  if (disable_fma && atoi(disable_fma)) {
    args.push_back("--fmad=false");
  }

  const char* ptxas_opt_level = getenv("PYTORCH_CUDA_FUSER_JIT_OPT_LEVEL");
  uint32_t jit_opt_level;

  std::vector<CUjit_option> options;
  std::vector<void*> option_vals;

  if (ptxas_opt_level) {
    int val = atoi(ptxas_opt_level);
    if (val <= 4 && val >= 0) {
      jit_opt_level = static_cast<uint32_t>(val);
      options.push_back(CU_JIT_OPTIMIZATION_LEVEL);
      option_vals.emplace_back(&jit_opt_level);
    } else {
      TORCH_WARN_ONCE(
          "acceptable range for PYTORCH_CUDA_FUSER_JIT_OPT_LEVEL is between 0 and 4, but received ",
          jit_opt_level,
          ", ignoring the option");
    }
  }

  // --include-path
  const std::string include_path = "--include-path=" + torchLibPath();
  args.push_back(include_path.c_str());

  at::globalContext().getNVRTC().nvrtcAddNameExpression(
      program, func_name.c_str());

  {
    FUSER_PERF_SCOPE("nvrtcCompileProgram");

    const auto result = at::globalContext().getNVRTC().nvrtcCompileProgram(
        program, args.size(), args.data());

    if (result != NVRTC_SUCCESS) {
      size_t logsize;
      at::globalContext().getNVRTC().nvrtcGetProgramLogSize(program, &logsize);
      std::vector<char> log(logsize);
      at::globalContext().getNVRTC().nvrtcGetProgramLog(program, log.data());

      TORCH_INTERNAL_ASSERT(
          false, code.c_str(), "\nCUDA NVRTC compile error: ", log.data());
    }

    AT_CUDA_NVRTC_CHECK(result);
  }

  const char* lowered_kernel_name = nullptr;
  at::globalContext().getNVRTC().nvrtcGetLoweredName(
      program, func_name.c_str(), &lowered_kernel_name);

  size_t ptx_size = 0;
  std::vector<char> ptx;

  {
    FUSER_PERF_SCOPE("get PTX");
    AT_CUDA_NVRTC_CHECK(
        at::globalContext().getNVRTC().nvrtcGetPTXSize(program, &ptx_size));
    ptx.resize(ptx_size);
    AT_CUDA_NVRTC_CHECK(
        at::globalContext().getNVRTC().nvrtcGetPTX(program, ptx.data()));
  }

  NvrtcFunction compiled_kernel_;

  // TODO: We do go through different code path, should investigate whether this
  // has an impact on generated binary.
#ifndef __HIP_PLATFORM_HCC__
  const char* prefix_env = getenv("PYTORCH_NVFUSER_CUBIN");
  if (prefix_env) {
    FUSER_PERF_SCOPE("load CUBIN");

    // Output ptx file
    std::stringstream ptx_file_name;
    ptx_file_name << prefix_env << "_" << id << ".ptx";
    std::ofstream myPtxFile(ptx_file_name.str().c_str(), std::ios::out);
    if (myPtxFile.is_open()) {
      myPtxFile.write(ptx.data(), ptx.size());
      myPtxFile.close();
    }

    CUlinkState linkState;

    AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuLinkCreate(
        0, nullptr, nullptr, &linkState));

    AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuLinkAddData(
        linkState,
        CU_JIT_INPUT_PTX,
        ptx.data(),
        ptx_size,
        "compiling PTX",
        options.size(),
        options.data(),
        option_vals.data()));

    size_t cubinSize;
    void* cubin;
    AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuLinkComplete(
        linkState, &cubin, &cubinSize));

    // Output binary file
    std::stringstream cubin_file_name;
    cubin_file_name << prefix_env << "_" << id << ".cubin";

    std::ofstream myCubinFile(
        cubin_file_name.str().c_str(), std::ios::out | std::ios::binary);

    if (myCubinFile.is_open()) {
      myCubinFile.write(static_cast<const char*>(cubin), cubinSize);
      myCubinFile.close();
    }
    // load compiled cubin
    AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuModuleLoadData(
        &(compiled_kernel_.module), cubin));
  } else {
    FUSER_PERF_SCOPE("load PTX");

    // load ptx directly
    AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuModuleLoadDataEx(
        &(compiled_kernel_.module),
        ptx.data(),
        options.size(),
        options.data(),
        option_vals.data()));
  }
#else
  // load ptx directly
  AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuModuleLoadData(
      &(compiled_kernel_.module), ptx.data()));

#endif
  AT_CUDA_DRIVER_CHECK(at::globalContext().getNVRTC().cuModuleGetFunction(
      &(compiled_kernel_.function),
      compiled_kernel_.module,
      lowered_kernel_name));

  return compiled_kernel_;
}

} // namespace executor_utils
} // namespace cuda
} // namespace fuser
} // namespace jit
} // namespace torch
