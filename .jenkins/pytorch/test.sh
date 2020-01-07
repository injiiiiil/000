#!/bin/bash

# Required environment variable: $BUILD_ENVIRONMENT
# (This is set by default in the Docker images we build, so you don't
# need to set it yourself.

# shellcheck disable=SC2034
COMPACT_JOB_NAME="${BUILD_ENVIRONMENT}"

source "$(dirname "${BASH_SOURCE[0]}")/common.sh"

echo "Testing pytorch"

if [ -n "${IN_CIRCLECI}" ]; then
  # TODO move this to docker
  pip_install unittest-xml-reporting

  if [[ "$BUILD_ENVIRONMENT" == *-xenial-cuda9-* ]]; then
    # TODO: move this to Docker
    sudo apt-get -qq update
    sudo apt-get -qq install --allow-downgrades --allow-change-held-packages libnccl-dev=2.2.13-1+cuda9.0 libnccl2=2.2.13-1+cuda9.0
  fi

  if [[ "$BUILD_ENVIRONMENT" == *-xenial-cuda8-* ]] || [[ "$BUILD_ENVIRONMENT" == *-xenial-cuda9-cudnn7-py2* ]]; then
    # TODO: move this to Docker
    sudo apt-get -qq update
    sudo apt-get -qq install --allow-downgrades --allow-change-held-packages openmpi-bin libopenmpi-dev
    sudo apt-get -qq install --no-install-recommends openssh-client openssh-server
    sudo mkdir -p /var/run/sshd
  fi

  if [[ "$BUILD_ENVIRONMENT" == *-slow-* ]]; then
    export PYTORCH_TEST_WITH_SLOW=1
    export PYTORCH_TEST_SKIP_FAST=1
  fi
fi

if [[ "$BUILD_ENVIRONMENT" == *rocm* ]]; then
  # TODO: Move this to Docker
  sudo apt-get -qq update
  sudo apt-get -qq install --no-install-recommends libsndfile1
fi

# --user breaks ppc64le builds and these packages are already in ppc64le docker
if [[ "$BUILD_ENVIRONMENT" != *ppc64le* ]]; then
  # JIT C++ extensions require ninja.
  pip_install --user ninja
  # ninja is installed in /var/lib/jenkins/.local/bin
  export PATH="/var/lib/jenkins/.local/bin:$PATH"

  # TODO: Please move this to Docker
  # The version is fixed to avoid flakiness: https://github.com/pytorch/pytorch/issues/31136
  pip_install --user "hypothesis==4.53.2"

  # TODO: move this to Docker
  PYTHON_VERSION=$(python -c 'import platform; print(platform.python_version())'|cut -c1)
  echo $PYTHON_VERSION
  # if [[ $PYTHON_VERSION == "2" ]]; then
  #   pip_install --user https://s3.amazonaws.com/ossci-linux/wheels/tensorboard-1.14.0a0-py2-none-any.whl
  # else
  #   pip_install --user https://s3.amazonaws.com/ossci-linux/wheels/tensorboard-1.14.0a0-py3-none-any.whl
  # fi
  pip_install --user tb-nightly
  # mypy will fail to install on Python <3.4.  In that case,
  # we just won't run these tests.
  pip_install --user mypy || true
fi

# faulthandler become built-in since 3.3
if [[ ! $(python -c "import sys; print(int(sys.version_info >= (3, 3)))") == "1" ]]; then
  pip_install --user faulthandler
fi

# DANGER WILL ROBINSON.  The LD_PRELOAD here could cause you problems
# if you're not careful.  Check this if you made some changes and the
# ASAN test is not working
if [[ "$BUILD_ENVIRONMENT" == *asan* ]]; then
    export ASAN_OPTIONS=detect_leaks=0:symbolize=1:strict_init_order=true
    export UBSAN_OPTIONS=print_stacktrace=1
    export PYTORCH_TEST_WITH_ASAN=1
    export PYTORCH_TEST_WITH_UBSAN=1
    # TODO: Figure out how to avoid hard-coding these paths
    export ASAN_SYMBOLIZER_PATH=/usr/lib/llvm-5.0/bin/llvm-symbolizer
    # NB: We load libtorch.so with RTLD_LOCAL.  This means that if
    # it gets loaded multiple times (as is the case with C++ extensions)
    # we will have multiple copies of the vptr floating around.  This
    # doesn't usually cause problems (as we try hard not to use RTTI)
    # but when UBSAN is turned on, it will do a bunch of virtual pointer
    # consistency checks which won't work correctly.  When this happens,
    # you get a violation like:
    #
    #    member call on address XXXXXX which does not point to an object of
    #    type 'std::_Sp_counted_base<__gnu_cxx::_Lock_policy::_S_atomic>'
    #    XXXXXX note: object is of type
    #    'std::_Sp_counted_ptr<torch::nn::LinearImpl*, (__gnu_cxx::_Lock_policy)2>'
    #
    # (NB: the textual types of the objects here are misleading, because
    # they actually line up; it just so happens that there's two copies
    # of the type info floating around in the address space, so they
    # don't pointer compare equal.  See also
    #   https://github.com/google/sanitizers/issues/1175
    #
    # This didn't use to be necessary, because historically we loaded
    # _C.so (and transitively, libtorch.so) using RTLD_GLOBAL. We
    # stopped doing that to promote better hygiene of C++ symbols,
    # but that means all weak symbols are going to get duplicated--this
    # especially applies to type info, which is almost always weak.  This
    # has implications for RTTI (which UBSAN is rightly flagging won't
    # work), but in our codebase, we don't use RTTI (because it doesn't
    # work in mobile).  However, UBSAN relies on UBSAN to detect vptr
    # confusion, so at least in this environment, we need our ducks in
    # order!
    #
    # By the way, if you arrange for libtorch.so to be loaded globally (e.g.,
    # using LD_PRELOAD), that would ensure subsequent loads of C++
    # extension modules consistently reference the type info in libtorch.so and
    # its dependencies (most notably, libc++.so).  Adding libtorch_python.so
    # to your LD_PRELOAD isn't really a good idea though, because it
    # depends on a ton of dynamic libraries that most programs aren't gonna
    # have.
    export LD_PRELOAD=/usr/lib/llvm-5.0/lib/clang/5.0.0/lib/linux/libclang_rt.asan-x86_64.so
    # Increase stack size, because ASAN red zones use more stack
    ulimit -s 81920

    (cd test && python -c "import torch")
    echo "The next three invocations are expected to crash; if they don't that means ASAN/UBSAN is misconfigured"
    (cd test && ! get_exit_code python -c "import torch; torch._C._crash_if_csrc_asan(3)")
    (cd test && ! get_exit_code python -c "import torch; torch._C._crash_if_csrc_ubsan(0)")
    (cd test && ! get_exit_code python -c "import torch; torch._C._crash_if_aten_asan(3)")
fi

if [[ "${BUILD_ENVIRONMENT}" == *-NO_AVX-* ]]; then
  export ATEN_CPU_CAPABILITY=default
elif [[ "${BUILD_ENVIRONMENT}" == *-NO_AVX2-* ]]; then
  export ATEN_CPU_CAPABILITY=avx
fi

test_python_nn() {
  time python test/run_test.py --include nn --verbose
  assert_git_not_dirty
}

test_python_ge_config_simple() {
  time python test/run_test.py --include jit_simple --verbose
  assert_git_not_dirty
}

test_python_ge_config_legacy() {
  time python test/run_test.py --include jit_legacy jit_fuser_legacy --verbose
  assert_git_not_dirty
}

test_python_all_except_nn() {
  time python test/run_test.py --exclude nn jit_simple jit_legacy jit_fuser_legacy --verbose --bring-to-front quantization quantized quantized_tensor quantized_nn_mods
  assert_git_not_dirty
}

test_aten() {
  # Test ATen
  # The following test(s) of ATen have already been skipped by caffe2 in rocm environment:
  # scalar_tensor_test, basic, native_test
  if ([[ "$BUILD_ENVIRONMENT" != *asan* ]] && [[ "$BUILD_ENVIRONMENT" != *rocm* ]]); then
    echo "Running ATen tests with pytorch lib"
    TORCH_LIB_PATH=$(python -c "import site; print(site.getsitepackages()[0])")/torch/lib
    # NB: the ATen test binaries don't have RPATH set, so it's necessary to
    # put the dynamic libraries somewhere were the dynamic linker can find them.
    # This is a bit of a hack.
    if [[ "$BUILD_ENVIRONMENT" == *ppc64le* ]]; then
      SUDO=sudo
    fi

    ${SUDO} ln -s "$TORCH_LIB_PATH"/libc10* build/bin
    ${SUDO} ln -s "$TORCH_LIB_PATH"/libcaffe2* build/bin
    ${SUDO} ln -s "$TORCH_LIB_PATH"/libmkldnn* build/bin
    ${SUDO} ln -s "$TORCH_LIB_PATH"/libnccl* build/bin

    ls build/bin
    aten/tools/run_tests.sh build/bin
    assert_git_not_dirty
  fi
}

test_torchvision() {
  pip_install --user git+https://github.com/pytorch/vision.git@44a5bae933655ed7ff798669a43452b833f9ce01
}

test_libtorch() {
  if [[ "$BUILD_ENVIRONMENT" != *rocm* ]]; then
    echo "Testing libtorch"
    python test/cpp/jit/tests_setup.py setup
    if [[ "$BUILD_ENVIRONMENT" == *cuda* ]]; then
      build/bin/test_jit
    else
      build/bin/test_jit "[cpu]"
    fi
    python test/cpp/jit/tests_setup.py shutdown
    python tools/download_mnist.py --quiet -d test/cpp/api/mnist
    OMP_NUM_THREADS=2 TORCH_CPP_TEST_MNIST_PATH="test/cpp/api/mnist" build/bin/test_api
    assert_git_not_dirty
  fi
}

test_custom_script_ops() {
  if [[ "$BUILD_ENVIRONMENT" != *rocm* ]] && [[ "$BUILD_ENVIRONMENT" != *asan* ]] ; then
    echo "Testing custom script operators"
    CUSTOM_OP_BUILD="$PWD/../custom-op-build"
    pushd test/custom_operator
    cp -a "$CUSTOM_OP_BUILD" build
    # Run tests Python-side and export a script module.
    python test_custom_ops.py -v
    python model.py --export-script-module=model.pt
    # Run tests C++-side and load the exported script module.
    build/test_custom_ops ./model.pt
    popd
    assert_git_not_dirty
  fi
}

test_xla() {
  export XLA_USE_XRT=1 XRT_DEVICE_MAP="CPU:0;/job:localservice/replica:0/task:0/device:XLA_CPU:0"
  export XRT_WORKERS="localservice:0;grpc://localhost:40934"
  pushd xla
  echo "Running Python Tests"
  ./test/run_tests.sh

  echo "Running MNIST Test"
  python test/test_train_mnist.py --tidy

  echo "Running C++ Tests"
  pushd test/cpp
  CC=clang-7 CXX=clang++-7 ./run_tests.sh
  popd
  assert_git_not_dirty
}

# Do NOT run this test before any other tests, like test_python_nn, etc.
# Because this function uninstalls the torch built from branch, and install
# nightly version.
test_backward_compatibility() {
  set -x
  pushd test/backward_compatibility
  python dump_all_function_schemas.py --filename new_schemas.txt
  pip_uninstall torch
  pip_install --pre torch -f https://download.pytorch.org/whl/nightly/cpu/torch_nightly.html
  python check_backward_compatibility.py --new-schemas new_schemas.txt
  popd
  set +x
  assert_git_not_dirty
}

if ! [[ "${BUILD_ENVIRONMENT}" == *libtorch* ]]; then
  (cd test && python -c "import torch; print(torch.__config__.show())")
  (cd test && python -c "import torch; print(torch.__config__.parallel_info())")
fi

if [[ "${BUILD_ENVIRONMENT}" == *backward* ]]; then
  test_backward_compatibility
  # Do NOT add tests after bc check tests, see its comment.
elif [[ "${BUILD_ENVIRONMENT}" == *xla* || "${JOB_BASE_NAME}" == *xla* ]]; then
  test_torchvision
  test_xla
elif [[ "${BUILD_ENVIRONMENT}" == *ge_config_legacy* || "${JOB_BASE_NAME}" == *ge_config_legacy* ]]; then
  test_python_ge_config_legacy
elif [[ "${BUILD_ENVIRONMENT}" == *ge_config_simple* || "${JOB_BASE_NAME}" == *ge_config_simple* ]]; then
  test_python_ge_config_simple
elif [[ "${BUILD_ENVIRONMENT}" == *libtorch* ]]; then
  # TODO: run some C++ tests
  echo "no-op at the moment"
elif [[ "${BUILD_ENVIRONMENT}" == *-test1 || "${JOB_BASE_NAME}" == *-test1 ]]; then
  test_torchvision
  test_python_nn
elif [[ "${BUILD_ENVIRONMENT}" == *-test2 || "${JOB_BASE_NAME}" == *-test2 ]]; then
  test_python_all_except_nn
  test_aten
  test_libtorch
  test_custom_script_ops
else
  test_torchvision
  test_python_nn
  test_python_all_except_nn
  test_aten
  test_libtorch
  test_custom_script_ops
fi
