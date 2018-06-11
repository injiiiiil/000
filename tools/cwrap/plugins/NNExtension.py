import os
from string import Template
from . import CWrapPlugin


MODULE_HEAD = """
#include <Python.h>
#include <exception>

#include "THP.h"
#include "torch/csrc/nn/type_checks.h"

#include <ATen/AutoGPU.h>

"""
REGISTER_METHOD_TEMPLATE = Template('  {"$name", (PyCFunction)$name, METH_STATIC | METH_VARARGS, NULL},\n')

MODULE_METHODS_TEMPLATE = Template("""
static PyMethodDef module_methods[] = {
$METHODS
  {NULL, NULL, 0, NULL}
};
""")


class NNExtension(CWrapPlugin):

    TYPE_UNPACK = {
        'THFloatTensor*': Template('THNN_FloatTensor_Unpack($arg)'),
        'THDoubleTensor*': Template('THNN_DoubleTensor_Unpack($arg)'),
        'THLongTensor*': Template('THNN_LongTensor_Unpack($arg)'),
        'THIntTensor*': Template('THNN_IntTensor_Unpack($arg)'),
        'THCudaHalfTensor*': Template('THNN_CudaHalfTensor_Unpack($arg)'),
        'THCudaTensor*': Template('THNN_CudaFloatTensor_Unpack($arg)'),
        'THCudaDoubleTensor*': Template('THNN_CudaDoubleTensor_Unpack($arg)'),
        'THCudaLongTensor*': Template('THNN_CudaLongTensor_Unpack($arg)'),
        'half': Template('THPHalfUtils_unpackReal($arg)'),
        'float': Template('THPFloatUtils_unpackReal($arg)'),
        'double': Template('THPDoubleUtils_unpackReal($arg)'),
        'bool': Template('($arg == Py_True ? true : false)'),
        'int': Template('THPUtils_unpackLong($arg)'),
        'long': Template('THPUtils_unpackLong($arg)'),
        'int64_t': Template('THPUtils_unpackLong($arg)'),
        'void*': Template('(void*)THPUtils_unpackLong($arg)'),
        'THGenerator*': Template('THPGenerator_TH_CData((THPGenerator*)$arg)'),
    }

    TYPE_CHECK = {
        'THFloatTensor*': Template('THNN_FloatTensor_Check($arg)'),
        'THDoubleTensor*': Template('THNN_DoubleTensor_Check($arg)'),
        'THLongTensor*': Template('THNN_LongTensor_Check($arg)'),
        'THIntTensor*': Template('THNN_IntTensor_Check($arg)'),
        'THCudaHalfTensor*': Template('THNN_CudaHalfTensor_Check($arg)'),
        'THCudaTensor*': Template('THNN_CudaFloatTensor_Check($arg)'),
        'THCudaDoubleTensor*': Template('THNN_CudaDoubleTensor_Check($arg)'),
        'THCudaLongTensor*': Template('THNN_CudaLongTensor_Check($arg)'),
        'half': Template('THPHalfUtils_checkReal($arg)'),
        'float': Template('THPFloatUtils_checkReal($arg)'),
        'double': Template('THPDoubleUtils_checkReal($arg)'),
        'bool': Template('PyBool_Check($arg)'),
        'int': Template('THPUtils_checkLong($arg)'),
        'long': Template('THPUtils_checkLong($arg)'),
        'int64_t': Template('THPUtils_checkLong($arg)'),
        'void*': Template('THPUtils_checkLong($arg)'),
        'THGenerator*': Template('(PyObject*)Py_TYPE($arg) == THPGeneratorClass'),
    }

    WRAPPER_TEMPLATE = Template("""
PyObject * $name(PyObject *_unused, PyObject *args)
{
  HANDLE_TH_ERRORS
  int __argcount = args ? PyTuple_Size(args) : 0;
    $options
  } else {
    THPUtils_invalidArguments(args, NULL, "$name", 1, $expected_args);
    return NULL;
  }
  END_HANDLE_TH_ERRORS
}
    """)

    TYPE_NAMES = {
        'THGenerator*': 'Generator',
        'THCudaHalfTensor*': 'torch.cuda.HalfTensor',
        'THCudaTensor*': 'torch.cuda.FloatTensor',
        'THCudaDoubleTensor*': 'torch.cuda.DoubleTensor',
        'THCudaLongTensor*': 'torch.cuda.LongTensor',
        'THDoubleTensor*': 'torch.DoubleTensor',
        'THFloatTensor*': 'torch.FloatTensor',
        'THBoolTensor*': 'torch.ByteTensor',
        'THLongTensor*': 'torch.LongTensor',
        'THIndexTensor*': 'torch.LongTensor',
        'THIntTensor*': 'torch.IntTensor',
        'THLongStorage*': 'torch.LongStorage',
        'long': 'int',
        'int64_t': 'int',
        'int': 'int',
        'real': 'float',
        'half': 'float',
        'double': 'float',
        'float': 'float',
        'accreal': 'float',
        'bool': 'bool',
        'void*': 'int',
    }

    def __init__(self, module_name):
        self.module_name = module_name
        self.declarations = []

    def process_full_file(self, code, template_path):
        with open(os.path.join(template_path, 'nn_tail.cpp'), 'r') as f:
            MODULE_TAIL = Template(f.read())

        short_name = self.module_name.split('.')[-1]
        new_code = MODULE_HEAD
        new_code += code
        new_code += self.declare_module_methods()
        new_code += MODULE_TAIL.substitute(full_name=self.module_name, short_name=short_name)
        return new_code

    def process_wrapper(self, code, declaration):
        self.declarations.append(declaration)
        return code

    def declare_module_methods(self):
        module_methods = ''
        for declaration in self.declarations:
            module_methods += REGISTER_METHOD_TEMPLATE.substitute(name=declaration['name'])
        return MODULE_METHODS_TEMPLATE.substitute(METHODS=module_methods)

    def get_type_unpack(self, arg, option):
        return self.TYPE_UNPACK.get(arg['type'], None)

    def get_type_check(self, arg, option):
        return self.TYPE_CHECK.get(arg['type'], None)

    def get_wrapper_template(self, declaration):
        arg_desc = []

        def describe_arg(arg):
            desc = self.TYPE_NAMES[arg['type']] + ' ' + arg['name']
            if arg.get('nullable'):
                return '[{} or None]'.format(desc)
            return desc
        for option in declaration['options']:
            option_desc = [describe_arg(arg)
                           for arg in option['arguments']
                           if not arg.get('ignore_check', False)]
            if option_desc:
                arg_desc.append('({})'.format(', '.join(option_desc)))
            else:
                arg_desc.append('no arguments')
        arg_desc.sort(key=len)
        arg_desc = ['"' + desc + '"' for desc in arg_desc]
        arg_str = ', '.join(arg_desc)
        return Template(self.WRAPPER_TEMPLATE.safe_substitute(expected_args=arg_str))
