#pragma once

#include <Python.h>
#include <memory>

#include "torch/csrc/autograd/variable.h"

struct THPGraph {
    PyObject_HEAD
    std::unique_ptr<torch::autograd::Graph> cdata;
};

extern PyObject *THPGraphClass;

PyObject * THPGraph_Wrap(const std::unique_ptr<torch::autograd::Graph> node);

inline bool THPGraph_Check(PyObject *obj)
{
  return THPGraphClass && PyObject_IsInstance(obj, THPGraphClass);
}

bool THPIR_initModule(PyObject *module);
