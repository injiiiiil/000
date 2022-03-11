#include <exception>

#ifndef MULTIPY_EXCEPTION_H
#define MULTIPY_INTERNAL_ASSERT_WITH_MESSAGE(condition, message)                 \
   if (!(condition)){                                               \
      throw std::runtime_error("Internal Assertion failed: (" + std::string(#condition) + "), "    \
      + "function " + __FUNCTION__                                \
      + ", file " + __FILE__                                      \
      + ", line " + std::to_string(__LINE__) + ".\n"                               \
      + "Please report bug to Pytorch.\n"                             \
      + message + "\n");                                          \
      }

#define MULTIPY_INTERNAL_ASSERT_NO_MESSAGE(condition)                          \
   MULTIPY_INTERNAL_ASSERT_WITH_MESSAGE(#condition, "")

#define MULTIPY_INTERNAL_ASSERT_(x, condition, message, FUNC, ...) FUNC

#define MULTIPY_INTERNAL_ASSERT(...) MULTIPY_INTERNAL_ASSERT_(,##__VA_ARGS__, \
   MULTIPY_INTERNAL_ASSERT_WITH_MESSAGE(__VA_ARGS__), \
   MULTIPY_INTERNAL_ASSERT_NO_MESSAGE(__VA_ARGS__));

#define MULTIPY_CHECK(condition, message)                 \
   if (!(condition)){                                               \
      throw std::runtime_error("Check failed: (" + std::string(#condition) + "), "    \
      + "function " + __FUNCTION__                                \
      + ", file " + __FILE__                                      \
      + ", line " + std::to_string(__LINE__) + ".\n"                               \
      + message + "\n");                                          \
      }

#endif // MULTIPY_EXCEPTION_H
