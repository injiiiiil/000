from collections import OrderedDict, namedtuple
import functools

import torch
from ..backends.thnn import backend as thnn_backend
from ..parameter import Parameter
import torch.utils.hooks as hooks


def _addindent(s_, numSpaces):
    s = s_.split('\n')
    # don't do anything for single-line stuff
    if len(s) == 1:
        return s_
    first = s.pop(0)
    s = [(numSpaces * ' ') + line for line in s]
    s = '\n'.join(s)
    s = first + '\n' + s
    return s


class Module(object):
    r"""Base class for all neural network modules.

    Your models should also subclass this class.

    Modules can also contain other Modules, allowing to nest them in
    a tree structure. You can assign the submodules as regular attributes::

        import torch.nn as nn
        import torch.nn.functional as F

        class Model(nn.Module):
            def __init__(self):
                super(Model, self).__init__()
                self.conv1 = nn.Conv2d(1, 20, 5)
                self.conv2 = nn.Conv2d(20, 20, 5)

            def forward(self, x):
               x = F.relu(self.conv1(x))
               return F.relu(self.conv2(x))

    Submodules assigned in this way will be registered, and will have their
    parameters converted too when you call `.cuda()`, etc.
    """

    dump_patches = False

    r"""This allows better BC support for :meth:`load_state_dict`. In
    :meth:`state_dict`, the version number will be saved as in the attribute
    `_versions` of the returned state dict, and thus pickled. `_versions` is a
    dictionary. Its keys follow the naming convention of state dict, but ends
    with key ``"version"`` and is associated with the version number of that
    submodule. For example, if ``module.state_dict()._version`` is

    {
        "version":         10,
        "m1.version":       0,
        "m1.m2.version":    3,
        "m1.m3.version":    1,
    }

    Then ``module`` has version number ``10`` and a submodule named ``m1`` with
    version number 0, which contains two other submodules with names ``m2`` and
    ``m3`` and version numbers ``3`` and ``1``.

    If new parameters/buffers are added/removed from a module, this number shall
    be bumped, and the module's `load_local_state_dict` method can compare the
    version number and do appropriate changes if the state dict is from before
    the change."""
    _version = 0

    def __init__(self):
        self._backend = thnn_backend
        self._parameters = OrderedDict()
        self._buffers = OrderedDict()
        self._backward_hooks = OrderedDict()
        self._forward_hooks = OrderedDict()
        self._forward_pre_hooks = OrderedDict()
        self._modules = OrderedDict()
        self.training = True

    def forward(self, *input):
        r"""Defines the computation performed at every call.

        Should be overridden by all subclasses.

        .. note::
            Although the recipe for forward pass needs to be defined within
            this function, one should call the :class:`Module` instance afterwards
            instead of this since the former takes care of running the
            registered hooks while the latter silently ignores them.
        """
        raise NotImplementedError

    def register_buffer(self, name, tensor):
        r"""Adds a persistent buffer to the module.

        This is typically used to register a buffer that should not to be
        considered a model parameter. For example, BatchNorm's ``running_mean``
        is not a parameter, but is part of the persistent state.

        Buffers can be accessed as attributes using given names.

        Args:
            name (string): name of the buffer. The buffer can be accessed
                from this module using the given name
            tensor (Tensor): buffer to be registered.

        Example::

            >>> self.register_buffer('running_mean', torch.zeros(num_features))

        """
        if hasattr(self, name) and name not in self._buffers:
            raise KeyError("attribute '{}' already exists".format(name))
        elif tensor is not None and not isinstance(tensor, torch.Tensor):
            raise TypeError("cannot assign '{}' object to buffer '{}' "
                            "(torch Tensor or None required)"
                            .format(torch.typename(tensor), name))
        else:
            self._buffers[name] = tensor

    def register_parameter(self, name, param):
        r"""Adds a parameter to the module.

        The parameter can be accessed as an attribute using given name.

        Args:
            name (string): name of the parameter. The parameter can be accessed
                from this module using the given name
            parameter (Parameter): parameter to be added to the module.
        """
        if '_parameters' not in self.__dict__:
            raise AttributeError(
                "cannot assign parameter before Module.__init__() call")

        if hasattr(self, name) and name not in self._parameters:
            raise KeyError("attribute '{}' already exists".format(name))

        if param is None:
            self._parameters[name] = None
        elif not isinstance(param, Parameter):
            raise TypeError("cannot assign '{}' object to parameter '{}' "
                            "(torch.nn.Parameter or None required)"
                            .format(torch.typename(param), name))
        elif param.grad_fn:
            raise ValueError(
                "Cannot assign non-leaf Tensor to parameter '{0}'. Model "
                "parameters must be created explicitly. To express '{0}' "
                "as a function of another Tensor, compute the value in "
                "the forward() method.".format(name))
        else:
            self._parameters[name] = param

    def add_module(self, name, module):
        r"""Adds a child module to the current module.

        The module can be accessed as an attribute using the given name.

        Args:
            name (string): name of the child module. The child module can be
                accessed from this module using the given name
            parameter (Module): child module to be added to the module.
        """
        if not isinstance(module, Module) and module is not None:
            raise TypeError("{} is not a Module subclass".format(
                torch.typename(module)))
        if hasattr(self, name) and name not in self._modules:
            raise KeyError("attribute '{}' already exists".format(name))
        self._modules[name] = module

    def _apply(self, fn):
        for module in self.children():
            module._apply(fn)

        for param in self._parameters.values():
            if param is not None:
                # Tensors stored in modules are graph leaves, and we don't
                # want to create copy nodes, so we have to unpack the data.
                param.data = fn(param.data)
                if param._grad is not None:
                    param._grad.data = fn(param._grad.data)

        for key, buf in self._buffers.items():
            if buf is not None:
                self._buffers[key] = fn(buf)

        return self

    def apply(self, fn):
        r"""Applies ``fn`` recursively to every submodule (as returned by ``.children()``)
        as well as self. Typical use includes initializing the parameters of a model
        (see also :ref:`torch-nn-init`).

        Args:
            fn (:class:`Module` -> None): function to be applied to each submodule

        Returns:
            Module: self

        Example::

            >>> def init_weights(m):
                    print(m)
                    if type(m) == nn.Linear:
                        m.weight.data.fill_(1.0)
                        print(m.weight)

            >>> net = nn.Sequential(nn.Linear(2, 2), nn.Linear(2, 2))
            >>> net.apply(init_weights)
            Linear(in_features=2, out_features=2, bias=True)

             1  1
             1  1
            [torch.FloatTensor of size (2,2)]

            Linear(in_features=2, out_features=2, bias=True)

             1  1
             1  1
            [torch.FloatTensor of size (2,2)]

            Sequential(
              (0): Linear(in_features=2, out_features=2, bias=True)
              (1): Linear(in_features=2, out_features=2, bias=True)
            )
            Sequential(
              (0): Linear(in_features=2, out_features=2, bias=True)
              (1): Linear(in_features=2, out_features=2, bias=True)
            )

        """
        for module in self.children():
            module.apply(fn)
        fn(self)
        return self

    def cuda(self, device=None):
        r"""Moves all model parameters and buffers to the GPU.

        This also makes associated parameters and buffers different objects. So
        it should be called before constructing optimizer if the module will
        live on GPU while being optimized.

        Arguments:
            device (int, optional): if specified, all parameters will be
                copied to that device

        Returns:
            Module: self
        """
        return self._apply(lambda t: t.cuda(device))

    def cpu(self):
        r"""Moves all model parameters and buffers to the CPU.

        Returns:
            Module: self
        """
        return self._apply(lambda t: t.cpu())

    def type(self, dst_type):
        r"""Casts all parameters and buffers to :attr:`dst_type`.

        Arguments:
            dst_type (type or string): the desired type

        Returns:
            Module: self
        """
        return self._apply(lambda t: t.type(dst_type))

    def float(self):
        r"""Casts all floating point parameters and buffers to float datatype.

        Returns:
            Module: self
        """
        return self._apply(lambda t: t.float() if t.is_floating_point() else t)

    def double(self):
        r"""Casts all floating point parameters and buffers to ``double`` datatype.

        Returns:
            Module: self
        """
        return self._apply(lambda t: t.double() if t.is_floating_point() else t)

    def half(self):
        r"""Casts all floating point parameters and buffers to ``half`` datatype.

        Returns:
            Module: self
        """
        return self._apply(lambda t: t.half() if t.is_floating_point() else t)

    def to(self, *args, **kwargs):
        r"""Moves and/or casts the parameters and buffers.

        This can be called with a :attr:`device` argument and/or a :attr:`dtype`
        argument. It has similar signature as :meth:`torch.Tensor.to`, except
        that this method doesn't take in :attr:`requires_grad` and only takes in
        floating point :attr:`dtype` s. In particular, this method will only
        cast the floating point parameters and buffers to :attr:`dtype`. It will
        still move the integral parameters and buffers to :attr:`device`, if
        that is given. See below for examples.

        Returns:
            Module: self

        Example::

            >>> linear = nn.Linear(2, 2)
            >>> linear.weight

            -0.1106  0.0493
             0.1250 -0.5175
            [torch.FloatTensor of size (2,2)]

            >>> linear.to(torch.double)
            Linear(in_features=2, out_features=2, bias=True)
            >>> linear.weight

            -0.1106  0.0493
             0.1250 -0.5175
            [torch.DoubleTensor of size (2,2)]

            >>> linear.to(torch.device("cuda"), dtype=torch.half)
            Linear(in_features=2, out_features=2, bias=True)
            >>> linear.weight

            -0.1107  0.0493
             0.1250 -0.5176
            [torch.cuda.HalfTensor of size (2,2) (GPU 0)]

            >>> linear.to("cpu")  # can also use string to represent device
            Linear(in_features=2, out_features=2, bias=True)
            >>> linear.weight

            -0.1107  0.0493
             0.1250 -0.5176
            [torch.HalfTensor of size (2,2)]

        """
        def arg_error():
            arg_reprs = list(repr(arg) for arg in args)
            for key, val in kwargs.items():
                arg_reprs.append("{}={}".format(key, val))
            return ValueError('module.to expects .to(device), .to(dtype) or '
                              '.to(device, dtype), where dtype is a floating '
                              'point type, but got .to({})'
                              .format(", ".join(arg_reprs)))

        nargs = len(args) + len(kwargs)
        device = dtype = None
        if nargs < 1 or nargs > 2:
            raise arg_error()
        else:
            for key, val in kwargs.items():
                if key == 'dtype':
                    dtype = kwargs['dtype']
                elif 'device' in kwargs:
                    device = kwargs['device']
                else:
                    raise arg_error()
            for arg in args:
                if isinstance(arg, torch.dtype):
                    if dtype is not None:
                        raise arg_error()
                    dtype = arg
                else:
                    if device is not None:
                        raise arg_error()
                    device = arg

        if dtype is not None:
            if not dtype.is_floating_point:
                raise arg_error()

            if device is None:
                return self._apply(lambda t: t.to(dtype) if t.is_floating_point() else t)
            else:
                return self._apply(lambda t: t.to(device, dtype) if t.is_floating_point() else t.to(device))

        else:
            return self._apply(lambda t: t.to(device))

    def register_backward_hook(self, hook):
        r"""Registers a backward hook on the module.

        The hook will be called every time the gradients with respect to module
        inputs are computed. The hook should have the following signature::

            hook(module, grad_input, grad_output) -> Tensor or None

        The :attr:`grad_input` and :attr:`grad_output` may be tuples if the
        module has multiple inputs or outputs. The hook should not modify its
        arguments, but it can optionally return a new gradient with respect to
        input that will be used in place of :attr:`grad_input` in subsequent
        computations.

        Returns:
            :class:`torch.utils.hooks.RemovableHandle`:
                a handle that can be used to remove the added hook by calling
                ``handle.remove()``
        """
        handle = hooks.RemovableHandle(self._backward_hooks)
        self._backward_hooks[handle.id] = hook
        return handle

    def register_forward_pre_hook(self, hook):
        r"""Registers a forward pre-hook on the module.

        The hook will be called every time before :func:`forward` is invoked.
        It should have the following signature::

            hook(module, input) -> None

        The hook should not modify the input.

        Returns:
            :class:`torch.utils.hooks.RemovableHandle`:
                a handle that can be used to remove the added hook by calling
                ``handle.remove()``
        """
        handle = hooks.RemovableHandle(self._forward_pre_hooks)
        self._forward_pre_hooks[handle.id] = hook
        return handle

    def register_forward_hook(self, hook):
        r"""Registers a forward hook on the module.

        The hook will be called every time after :func:`forward` has computed an output.
        It should have the following signature::

            hook(module, input, output) -> None

        The hook should not modify the input or output.

        Returns:
            :class:`torch.utils.hooks.RemovableHandle`:
                a handle that can be used to remove the added hook by calling
                ``handle.remove()``
        """
        handle = hooks.RemovableHandle(self._forward_hooks)
        self._forward_hooks[handle.id] = hook
        return handle

    def _tracing_name(self, tracing_state):
        if not tracing_state._traced_module_stack:
            return None
        module = tracing_state._traced_module_stack[-1]
        for name, child in module.named_children():
            if child is self:
                return name
        return None

    def _slow_forward(self, *input, **kwargs):
        input_vars = tuple(torch.autograd.function._iter_tensors(input))
        tracing_state = torch.jit.get_tracing_state(input_vars)
        if not tracing_state:
            return self.forward(*input, **kwargs)
        if not hasattr(tracing_state, '_traced_module_stack'):
            tracing_state._traced_module_stack = []
        name = self._tracing_name(tracing_state)
        if name:
            tracing_state.push_scope('%s[%s]' % (self.__class__.__name__, name))
        else:
            tracing_state.push_scope(self.__class__.__name__)
        tracing_state._traced_module_stack.append(self)
        try:
            result = self.forward(*input, **kwargs)
        finally:
            tracing_state.pop_scope()
            tracing_state._traced_module_stack.pop()
        return result

    def __call__(self, *input, **kwargs):
        for hook in self._forward_pre_hooks.values():
            hook(self, input)
        if torch.jit._tracing:
            result = self._slow_forward(*input, **kwargs)
        else:
            result = self.forward(*input, **kwargs)
        for hook in self._forward_hooks.values():
            hook_result = hook(self, input, result)
            if hook_result is not None:
                raise RuntimeError(
                    "forward hooks should never return any values, but '{}'"
                    "didn't return None".format(hook))
        if len(self._backward_hooks) > 0:
            var = result
            while not isinstance(var, torch.Tensor):
                if isinstance(var, dict):
                    var = next((v for v in var.values() if isinstance(v, torch.Tensor)))
                else:
                    var = var[0]
            grad_fn = var.grad_fn
            if grad_fn is not None:
                for hook in self._backward_hooks.values():
                    wrapper = functools.partial(hook, self)
                    functools.update_wrapper(wrapper, hook)
                    grad_fn.register_hook(wrapper)
        return result

    def __setstate__(self, state):
        self.__dict__.update(state)
        if '_forward_pre_hooks' not in self.__dict__:
            self._forward_pre_hooks = OrderedDict()

    def __getattr__(self, name):
        if '_parameters' in self.__dict__:
            _parameters = self.__dict__['_parameters']
            if name in _parameters:
                return _parameters[name]
        if '_buffers' in self.__dict__:
            _buffers = self.__dict__['_buffers']
            if name in _buffers:
                return _buffers[name]
        if '_modules' in self.__dict__:
            modules = self.__dict__['_modules']
            if name in modules:
                return modules[name]
        raise AttributeError("'{}' object has no attribute '{}'".format(
            type(self).__name__, name))

    def __setattr__(self, name, value):
        def remove_from(*dicts):
            for d in dicts:
                if name in d:
                    del d[name]

        params = self.__dict__.get('_parameters')
        if isinstance(value, Parameter):
            if params is None:
                raise AttributeError(
                    "cannot assign parameters before Module.__init__() call")
            remove_from(self.__dict__, self._buffers, self._modules)
            self.register_parameter(name, value)
        elif params is not None and name in params:
            if value is not None:
                raise TypeError("cannot assign '{}' as parameter '{}' "
                                "(torch.nn.Parameter or None expected)"
                                .format(torch.typename(value), name))
            self.register_parameter(name, value)
        else:
            modules = self.__dict__.get('_modules')
            if isinstance(value, Module):
                if modules is None:
                    raise AttributeError(
                        "cannot assign module before Module.__init__() call")
                remove_from(self.__dict__, self._parameters, self._buffers)
                modules[name] = value
            elif modules is not None and name in modules:
                if value is not None:
                    raise TypeError("cannot assign '{}' as child module '{}' "
                                    "(torch.nn.Module or None expected)"
                                    .format(torch.typename(value), name))
                modules[name] = value
            else:
                buffers = self.__dict__.get('_buffers')
                if buffers is not None and name in buffers:
                    if value is not None and not isinstance(value, torch.Tensor):
                        raise TypeError("cannot assign '{}' as buffer '{}' "
                                        "(torch.Tensor or None expected)"
                                        .format(torch.typename(value), name))
                    buffers[name] = value
                else:
                    object.__setattr__(self, name, value)

    def __delattr__(self, name):
        if name in self._parameters:
            del self._parameters[name]
        elif name in self._buffers:
            del self._buffers[name]
        elif name in self._modules:
            del self._modules[name]
        else:
            object.__delattr__(self, name)

    def state_dict(self, destination=None, prefix='', keep_vars=False, no_child=False):
        r"""Returns a dictionary containing a whole state of the module.

        Both parameters and persistent buffers (e.g. running averages) are
        included. Keys are corresponding parameter and buffer names.

        When keep_vars is ``True``, it returns a Tensor for each parameter
        (rather than a Tensor).

        Args:
            destination (dict, optional):
                if not ``None``, the return dictionary is stored into
                :attr:`destination`. Default: None
            prefix (string, optional): Adds a prefix to the key (name) of every
                parameter and buffer in the result dictionary. Default: ''
            keep_vars (bool, optional): if ``True``, returns a Tensor for each
                parameter. If ``False``, returns a Tensor for each parameter.
                Default: ``False``
            no_child (bool, optional): if ``True``, excludes parameters and
                buffers of submodules in the returned state dict.
                Default: ``False``

        Returns:
            dict:
                a dictionary containing a whole state of the module

        Example::

            >>> module.state_dict().keys()
            ['bias', 'weight']

        """
        if destination is None:
            destination = OrderedDict()
            destination._versions = OrderedDict()
        if hasattr(destination, '_versions'):
            destination._versions[prefix + 'version'] = self._version
        for name, param in self._parameters.items():
            if param is not None:
                destination[prefix + name] = param if keep_vars else param.data
        for name, buf in self._buffers.items():
            if buf is not None:
                destination[prefix + name] = buf
        if not no_child:
            for name, module in self._modules.items():
                if module is not None:
                    module.state_dict(destination, prefix + name + '.', keep_vars=keep_vars)
        return destination

    def load_local_state_dict(self, local_state_dict, version, strict):
        r"""Copies parameters and buffers from :attr:`local_state_dict` into
        only this module, but not its descendants. This is called on every
        submodule in :meth:`~torch.nn.Module.load_state_dict`. This method
        can be overridden by subclasses to achieve class-specific backward
        compatible loading using the version number :attr:`version`.

        .. note::
            :attr:`local_state_dict` is a different object that the input
            :attr:`state_dict` to :meth:`~torch.nn.Module.load_state_dict`. So
            it can be modified freely if needed.

        Arguments:
            local_state_dict (dict): A dict containing local parameters and
                persistent buffers of this module, but not of its descendants.
            version (int): The version number associated with the input
                :attr:`local_state_dict`. If the input state dict was created
                without a version number, this will be ``None``
            strict (bool): Whether to strictly enforce that the keys in
                :attr:`state_dict` match the keys returned by this module's
                :meth:`~torch.nn.Module.state_dict` function.
        """
        own_local_state = self.state_dict(no_child=True)

        unexpected = []
        for name, param in local_state_dict.items():
            if name in own_local_state:
                if isinstance(param, Parameter):
                    # backwards compatibility for serialized parameters
                    param = param.data
                try:
                    own_local_state[name].copy_(param)
                except Exception:
                    raise RuntimeError('While copying the parameter named {}, '
                                       'whose dimensions in the model are {} and '
                                       'whose dimensions in the checkpoint are {}.'
                                       .format(name, own_local_state[name].size(), param.size()))
            elif strict:
                unexpected.append(name)

        if strict:
            missing = set(own_local_state.keys()) - set(local_state_dict.keys())
            error_msg = ''
            if len(unexpected) > 0:
                error_msg += 'Unexpected key(s) in state_dict: {}. '.format(
                    ', '.join('"{}"'.format(k) for k in unexpected))
            if len(missing) > 0:
                error_msg += 'Missing key(s) in state_dict: {}. '.format(
                    ', '.join('"{}"'.format(k) for k in missing))
            if len(error_msg) > 0:
                raise KeyError('Error loading state_dict for {}: {}'.format(
                               self.__class__.__name__, error_msg))

    def load_state_dict(self, state_dict, strict=True):
        r"""Copies parameters and buffers from :attr:`state_dict` into
        this module and its descendants. If :attr:`strict` is ``True``, then
        the keys of :attr:`state_dict` must exactly match the keys returned
        by this module's :meth:`~torch.nn.Module.state_dict` function.

        Arguments:
            state_dict (dict): A dict containing parameters and
                persistent buffers.
            strict (bool, optional): Whether to strictly enforce that the keys
                in :attr:`state_dict` match the keys returned by this module's
                :meth:`~torch.nn.Module.state_dict` function. Default: ``True``
        """
        versions = getattr(state_dict, '_versions', {})

        # build a tree
        # each node represents a (sub)module as a namedtuple
        #     (
        #       module            [nn.Module],
        #       path              [list<string>],
        #       local_state_dict  [dict<string, Tensor>],
        #       children          [dict<string, Node>],
        #     )
        #
        #  e.g. for state_dict with keys {"m1.resblock.weight", "m1.resblock.conv.weight"},
        #       the Node for m1.resblock might be
        #       (
        #           module=ResnetBlock,
        #           path=["m1", "resblock"],
        #           local_state_dict={"weight": some_tensor},
        #           children={"conv": some_node},
        #        )
        #

        Node = namedtuple('Node', ['module', 'path', 'local_state_dict', 'children'])

        state_tree = Node(self, [], {}, {})

        unexpected_modules = set()

        def add_to_tree(path, value, idx=0, root=state_tree):
            if len(path) == idx + 1:  # at [..., name_of_param <- here]
                root.local_state_dict[path[idx]] = value
            else:
                key = path[idx]       # at [..., name_of_submodule <- here, ..., name_of_param]
                if key not in root.children:
                    if key in root.module._modules:
                        # make a subtree for this newly seen submodule
                        subtree = Node(root.module._modules[key], path[:(idx + 1)], {}, {})
                        root.children[key] = subtree
                    elif strict:
                        unexpected_modules.add('.'.join(path[:(idx + 1)]))
                        return
                add_to_tree(path, value, idx + 1, root.children[key])

        for key, value in state_dict.items():
            add_to_tree(key.split('.'), value)

        if strict:
            missing_modules = []

            def validate_tree(root=state_tree):
                for name, module in root.module._modules.items():
                    if module is not None and (len(module._parameters) > 0 or len(module._buffers) > 0):
                        if name not in root.children:
                            missing_modules.append(('.'.join(root.path + [name]), module.__class__.__name__))
                        else:
                            validate_tree(root.children[name])

            validate_tree()
            error_msg = ''
            if len(unexpected_modules) > 0:
                error_msg += 'Unexpected module key(s) in state_dict: {}. '.format(
                    ', '.join('"{}"'.format(k) for k in unexpected_modules))
            if len(missing_modules) > 0:
                error_msg += 'Missing module key(s) in state_dict: {}. '.format(
                    ', '.join('"{}" of type {}'.format(name, cls) for name, cls in missing_modules))
            if len(error_msg) > 0:
                raise KeyError('Error loading state_dict for {}: {}'.format(
                               self.__class__.__name__, error_msg))

        def load_tree(root=state_tree):
            for subtree in root.children.values():
                load_tree(subtree)
            version = versions.get('.'.join(root.path + ['version']), None)
            root.module.load_local_state_dict(root.local_state_dict, version, strict)

        load_tree()

    def parameters(self):
        r"""Returns an iterator over module parameters.

        This is typically passed to an optimizer.

        Yields:
            Parameter: module parameter

        Example::

            >>> for param in model.parameters():
            >>>     print(type(param.data), param.size())
            <class 'torch.FloatTensor'> (20L,)
            <class 'torch.FloatTensor'> (20L, 1L, 5L, 5L)

        """
        for name, param in self.named_parameters():
            yield param

    def named_parameters(self, memo=None, prefix=''):
        r"""Returns an iterator over module parameters, yielding both the
        name of the parameter as well as the parameter itself

        Yields:
            (string, Parameter): Tuple containing the name and parameter

        Example::

            >>> for name, param in self.named_parameters():
            >>>    if name in ['bias']:
            >>>        print(param.size())

        """
        if memo is None:
            memo = set()
        for name, p in self._parameters.items():
            if p is not None and p not in memo:
                memo.add(p)
                yield prefix + ('.' if prefix else '') + name, p
        for mname, module in self.named_children():
            submodule_prefix = prefix + ('.' if prefix else '') + mname
            for name, p in module.named_parameters(memo, submodule_prefix):
                yield name, p

    def _all_buffers(self, memo=None):
        if memo is None:
            memo = set()
        for name, b in self._buffers.items():
            if b is not None and b not in memo:
                memo.add(b)
                yield b
        for module in self.children():
            for b in module._all_buffers(memo):
                yield b

    def children(self):
        r"""Returns an iterator over immediate children modules.

        Yields:
            Module: a child module
        """
        for name, module in self.named_children():
            yield module

    def named_children(self):
        r"""Returns an iterator over immediate children modules, yielding both
        the name of the module as well as the module itself.

        Yields:
            (string, Module): Tuple containing a name and child module

        Example::

            >>> for name, module in model.named_children():
            >>>     if name in ['conv4', 'conv5']:
            >>>         print(module)

        """
        memo = set()
        for name, module in self._modules.items():
            if module is not None and module not in memo:
                memo.add(module)
                yield name, module

    def modules(self):
        r"""Returns an iterator over all modules in the network.

        Yields:
            Module: a module in the network

        Note:
            Duplicate modules are returned only once. In the following
            example, ``l`` will be returned only once.

        Example::

            >>> l = nn.Linear(2, 2)
            >>> net = nn.Sequential(l, l)
            >>> for idx, m in enumerate(net.modules()):
                    print(idx, '->', m)

            0 -> Sequential (
              (0): Linear (2 -> 2)
              (1): Linear (2 -> 2)
            )
            1 -> Linear (2 -> 2)

        """
        for name, module in self.named_modules():
            yield module

    def named_modules(self, memo=None, prefix=''):
        r"""Returns an iterator over all modules in the network, yielding
        both the name of the module as well as the module itself.

        Yields:
            (string, Module): Tuple of name and module

        Note:
            Duplicate modules are returned only once. In the following
            example, ``l`` will be returned only once.

        Example::

            >>> l = nn.Linear(2, 2)
            >>> net = nn.Sequential(l, l)
            >>> for idx, m in enumerate(net.named_modules()):
                    print(idx, '->', m)

            0 -> ('', Sequential (
              (0): Linear (2 -> 2)
              (1): Linear (2 -> 2)
            ))
            1 -> ('0', Linear (2 -> 2))

        """

        if memo is None:
            memo = set()
        if self not in memo:
            memo.add(self)
            yield prefix, self
            for name, module in self._modules.items():
                if module is None:
                    continue
                submodule_prefix = prefix + ('.' if prefix else '') + name
                for m in module.named_modules(memo, submodule_prefix):
                    yield m

    def train(self, mode=True):
        r"""Sets the module in training mode.

        This has any effect only on certain modules. See documentations of
        particular modules for details of their behaviors in training/evaluation
        mode, if they are affected, e.g. :class:`Dropout`, :class:`BatchNorm`,
        etc.

        Returns:
            Module: self
        """
        self.training = mode
        for module in self.children():
            module.train(mode)
        return self

    def eval(self):
        r"""Sets the module in evaluation mode.

        This has any effect only on certain modules. See documentations of
        particular modules for details of their behaviors in training/evaluation
        mode, if they are affected, e.g. :class:`Dropout`, :class:`BatchNorm`,
        etc.
        """
        return self.train(False)

    def zero_grad(self):
        r"""Sets gradients of all model parameters to zero."""
        for p in self.parameters():
            if p.grad is not None:
                p.grad.detach_()
                p.grad.zero_()

    def share_memory(self):
        return self._apply(lambda t: t.share_memory_())

    def _get_name(self):
        return self.__class__.__name__

    def extra_repr(self):
        r"""Set the extra representation of the module

        To print customized extra information, you should reimplement
        this method in your own modules. Both single-line and multi-line
        strings are acceptable.
        """
        return ''

    def __repr__(self):
        # We treat the extra repr like the sub-module, one item per line
        extra_lines = []
        extra_repr = self.extra_repr()
        # empty string will be split into list ['']
        if extra_repr:
            extra_lines = extra_repr.split('\n')
        child_lines = []
        for key, module in self._modules.items():
            mod_str = repr(module)
            mod_str = _addindent(mod_str, 2)
            child_lines.append('(' + key + '): ' + mod_str)
        lines = extra_lines + child_lines

        main_str = self._get_name() + '('
        if lines:
            # simple one-liner info, which most builtin Modules will use
            if len(extra_lines) == 1 and not child_lines:
                main_str += extra_lines[0]
            else:
                main_str += '\n  ' + '\n  '.join(lines) + '\n'

        main_str += ')'
        return main_str

    def __dir__(self):
        module_attrs = dir(self.__class__)
        attrs = list(self.__dict__.keys())
        parameters = list(self._parameters.keys())
        modules = list(self._modules.keys())
        buffers = list(self._buffers.keys())
        keys = module_attrs + attrs + parameters + modules + buffers

        # Eliminate attrs that are not legal Python variable names
        keys = [key for key in keys if not key[0].isdigit()]

        return sorted(keys)
