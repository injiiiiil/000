#include <ATen/core/Error.h>
#include <ATen/core/Backtrace.h>

#include <iostream>
#include <string>
#include <numeric>

namespace at {

namespace detail {

std::string StripBasename(const std::string &full_path) {
  const char kSeparator = '/';
  size_t pos = full_path.rfind(kSeparator);
  if (pos != std::string::npos) {
    return full_path.substr(pos + 1, std::string::npos);
  } else {
    return full_path;
  }
}

} // namespace detail

std::ostream& operator<<(std::ostream& out, const SourceLocation& loc) {
  out << loc.function << " at " << loc.file << ":" << loc.line;
  return out;
}

Error::Error(const std::string& new_msg, const std::string& backtrace, const void* caller)
    : msg_stack_{new_msg}
    , backtrace_(backtrace)
    , caller_(caller) {
  msg_ = msg();
  msg_without_backtrace_ = msg_without_backtrace();
}

// PyTorch-style error message
Error::Error(SourceLocation source_location, const std::string& msg)
  : Error(msg, str(" (", source_location, ")\n", get_backtrace(/*frames_to_skip=*/2))) {}

// Caffe2-style error message
Error::Error(const char* file, const int line, const char* condition, const std::string& msg, const void* caller)
  : Error(str("[enforce fail at ", detail::StripBasename(file), ":", line, "] ", condition, ". ", msg),
          str("\n", get_backtrace(/*frames_to_skip=*/2)),
          caller) {}

std::string Error::msg() const {
  return std::accumulate(msg_stack_.begin(), msg_stack_.end(), std::string("")) + backtrace_;
}

std::string Error::msg_without_backtrace() const {
  return std::accumulate(msg_stack_.begin(), msg_stack_.end(), std::string(""));
}

void Error::AppendMessage(const std::string& new_msg) {
  msg_stack_.push_back(new_msg);
  // Refresh the cache
  // TODO: Calling AppendMessage O(n) times has O(n^2) cost.  We can fix
  // this perf problem by populating the fields lazily... if this ever
  // actually is a problem.
  msg_ = msg();
  msg_without_backtrace_ = msg_without_backtrace();
}

void Warning::warn(SourceLocation source_location, std::string msg) {
  warning_handler_(source_location, msg.c_str());
}

void Warning::set_warning_handler(handler_t handler) {
  warning_handler_ = handler;
}

void Warning::print_warning(
    const SourceLocation& source_location,
    const char* msg) {
  std::cerr << "Warning: " << msg << " (" << source_location << ")\n";
}

Warning::handler_t Warning::warning_handler_ = &Warning::print_warning;

} // namespace at
