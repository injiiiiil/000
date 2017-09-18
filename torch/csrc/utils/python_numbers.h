#pragma once

#include <Python.h>

// largest integer that can be represented consecutively in a double
const int64_t DOUBLE_INT_MAX = 9007199254740992;

inline bool THPUtils_checkLong(PyObject* obj) {
#if PY_MAJOR_VERSION == 2
  return (PyLong_Check(obj) || PyInt_Check(obj)) && !PyBool_Check(obj);
#else
  return PyLong_Check(obj) && !PyBool_Check(obj);
#endif
}

inline int64_t THPUtils_unpackLong(PyObject* obj) {
  if (PyLong_Check(obj)) {
    int overflow;
    int64_t value = PyLong_AsLongLongAndOverflow(obj, &overflow);
    if (overflow != 0) {
      throw std::runtime_error("Overflow when unpacking 64-bit integer type");
    }
    return (int64_t)value;
  }
#if PY_MAJOR_VERSION == 2
  if (PyInt_Check(obj)) {
    return PyInt_AS_LONG(obj);
  }
#endif
  throw std::runtime_error("Could not unpack 64-bit integer type");
}

inline bool THPUtils_checkDouble(PyObject* obj) {
#if PY_MAJOR_VERSION == 2
  return PyFloat_Check(obj) || PyLong_Check(obj) || PyInt_Check(obj);
#else
  return PyFloat_Check(obj) || PyLong_Check(obj);
#endif
}

inline double THPUtils_unpackDouble(PyObject* obj) {
  if (PyFloat_Check(obj)) {
    return PyFloat_AS_DOUBLE(obj);
  }
  if (PyLong_Check(obj)) {
    int overflow;
    int64_t value = PyLong_AsLongLongAndOverflow(obj, &overflow);
    if (overflow != 0) {
      throw std::runtime_error("Overflow when unpacking 64-bit floating point type");
    }
    if (value > DOUBLE_INT_MAX || value < -DOUBLE_INT_MAX) {
      throw std::runtime_error("Precision loss when unpacking 64-bit floating point type");
    }
    return (double)value;
  }
#if PY_MAJOR_VERSION == 2
  if (PyInt_Check(obj)) {
    return (double)PyInt_AS_LONG(obj);
  }
#endif
  throw std::runtime_error("Could not unpack 64-bit floating point type");
}
