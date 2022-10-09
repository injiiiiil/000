#pragma once

#include <c10/core/Device.h>
#include <c10/util/Exception.h>

#include <c10/util/Registry.h>

#include <cstddef>
#include <functional>
#include <memory>

namespace at {
class Context;
}

namespace at {

constexpr const char* XPU_HELP =
    "The XPU backend requires Intel Extension for Pytorch;"
    "this error has occurred because you are trying "
    "to use some XPU's functionality, but the Intel Extension for Pytorch has not been "
    "loaded for some reason. The Intel Extension for Pytorch MUST "
    "be loaded, EVEN IF you don't directly use any symbols from that!";

struct TORCH_API XPUHooksInterface {
  virtual ~XPUHooksInterface() {}

  virtual void initXPU() const {
    TORCH_CHECK(
        false,
        "Cannot initialize XPU without Intel Extension for Pytorch.",
        XPU_HELP);
  }

  virtual bool hasXPU() const {
    return false;
  }

  virtual std::string showConfig() const {
    TORCH_CHECK(
        false,
        "Cannot query detailed XPU version without Intel Extension for Pytorch. ",
        XPU_HELP);
  }

  virtual DeviceIndex device_count() const {
    return 0;
  }

  virtual DeviceIndex current_device() const {
    return -1;
  }
};

struct TORCH_API XPUHooksArgs {};

C10_DECLARE_REGISTRY(XPUHooksRegistry, XPUHooksInterface, XPUHooksArgs);
#define REGISTER_XPU_HOOKS(clsname) \
  C10_REGISTER_CLASS(XPUHooksRegistry, clsname, clsname)

namespace detail {
TORCH_API const XPUHooksInterface& getXPUHooks();
} // namespace detail
} // namespace at
