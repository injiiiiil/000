from __future__ import absolute_import, division, print_function, unicode_literals
import copy
import unittest

try:
    import torchvision
    HAS_TORCHVISION = True
except ImportError:
    HAS_TORCHVISION = False

skipIfNoTorchVision = unittest.skipIf(not HAS_TORCHVISION, "no torchvision")

import torch
import torch.jit
import torch.backends.mkldnn
from torch.utils import mkldnn as mkldnn_utils
from torch.testing._internal.common_utils import TestCase, run_tests, TemporaryFileName

from torch.autograd.gradcheck import gradgradcheck, gradcheck
from hypothesis import given
from hypothesis import strategies as st

dtype2prec = {torch.float: 1e-5, torch.bfloat16: 1e-2}

# Comment the line below to find out the CI machines having MKL-DNN build disabled
@unittest.skipIf(not torch._C.has_mkldnn, "MKL-DNN build is disabled")
class TestMkldnn(TestCase):
    @given(stype=st.sampled_from((torch.float, torch.bfloat16)),
           otype=st.sampled_from((torch.float, torch.bfloat16)))
    def test_conversion(self, stype, otype):
        for cpu_tensor in [torch.randn((1, 2, 3, 4),
                                       dtype=torch.float, device=torch.device('cpu')),
                           torch.randn((1, 2, 3, 4, 5),
                                       dtype=torch.float, device=torch.device('cpu'))[:, :, :, :, 1]]:
            cpu_tensor.requires_grad_()
            mkldnn_tensor = cpu_tensor.to_mkldnn(stype)
            cpu_tensor_1 = mkldnn_tensor.to_dense(otype)
            self.assertEqual(cpu_tensor, cpu_tensor_1,
                dtype2prec[torch.bfloat16 if torch.bfloat16 in [stype, otype] else torch.float])
            self.assertEqual(mkldnn_tensor.dtype, stype)
            self.assertEqual(mkldnn_tensor.device, torch.device('cpu'))
            self.assertEqual(mkldnn_tensor.size(), torch.Size([1, 2, 3, 4]))
            self.assertEqual(mkldnn_tensor.numel(), cpu_tensor.numel())
            self.assertEqual(mkldnn_tensor.element_size(), cpu_tensor.to(stype).element_size())
            self.assertRaisesRegex(RuntimeError,
                                   "Cannot access data pointer of Tensor that doesn't have storage",
                                   lambda: mkldnn_tensor.data_ptr() != 0)

    def test_unsupported(self):
        # unsupported types and unsupported types with gpu
        for dtype in [torch.double, torch.half, torch.uint8, torch.int8,
                      torch.short, torch.int, torch.long]:
            with self.assertRaises(RuntimeError) as context:
                torch.randn(1, 2, 3, 4, dtype=dtype, device=torch.device('cpu')).to_mkldnn()
            if torch.cuda.is_available():
                with self.assertRaises(RuntimeError) as context:
                    torch.randn(1, 2, 3, 4, dtype=dtype, device=torch.device('cuda')).to_mkldnn()
        # supported type with gpu
        if torch.cuda.is_available():
            with self.assertRaises(RuntimeError) as context:
                torch.randn(1, 2, 3, 4, dtype=torch.float, device=torch.device('cuda')).to_mkldnn()
        # some factory functions
        for creator in [torch.ones, torch.randn, torch.rand]:
            with self.assertRaises(RuntimeError) as context:
                creator(1, 2, 3, 4, dtype=torch.float, device=torch.device('cpu'), layout=torch._mkldnn)

    def test_autograd_to_mkldnn(self):
        # MKLDNN only supports float32
        root = torch.randn(4, 5, dtype=torch.float32, requires_grad=True)

        def func(root):
            return root.to_mkldnn().to_dense()

        # because MKLDNN only supports float32, we need to lessen the precision.
        # these numbers are just empirical results that seem to work.
        self.assertWarnsRegex(lambda: gradcheck(func, [root], atol=4e-2, rtol=1e-2),
                              'double precision floating point')
        self.assertWarnsRegex(lambda: gradgradcheck(func, [root], atol=4e-2, rtol=1e-2),
                              'double precision floating point')

    def test_autograd_from_mkldnn(self):
        # MKLDNN only supports float32
        root = torch.randn(4, 5, dtype=torch.float32).to_mkldnn().requires_grad_()

        def func(root):
            return root.to_dense()

        # because MKLDNN only supports float32, we need to lessen the precision.
        # these numbers are just empirical results that seem to work.
        self.assertWarnsRegex(lambda: gradcheck(func, [root], atol=4e-2, rtol=1e-2),
                              'double precision floating point')

    def test_detach(self):
        root = torch.randn(4, 5, dtype=torch.float32).to_mkldnn().requires_grad_()

        detach = root.detach()
        self.assertEqual((4, 5), detach.size())
        self.assertFalse(detach.requires_grad)
        self.assertTrue(root.requires_grad)

        detach_ = root.detach_()
        self.assertEqual((4, 5), detach_.size())
        self.assertFalse(detach_.requires_grad)
        self.assertFalse(root.requires_grad)

    def test_repr(self):
        self.assertTrue("layout=torch._mkldnn" in str(torch.randn((1, 2, 3, 4),
                                                                  dtype=torch.float, device=torch.device('cpu')).to_mkldnn()))

    def test_conv2d(self):
        for groups in [1, 4]:
            N = torch.randint(3, 10, (1,)).item()
            C = torch.randint(1, 3, (1,)).item() * groups
            M = torch.randint(1, 3, (1,)).item() * groups
            x = torch.randn(N, C, 224, 224, dtype=torch.float32)
            for bias in [True, False]:
                conv2d = torch.nn.Conv2d(in_channels=C,
                                         out_channels=M,
                                         kernel_size=3,
                                         stride=2,
                                         padding=1,
                                         bias=bias,
                                         groups=groups).float()
                mkldnn_conv2d = mkldnn_utils.to_mkldnn(copy.deepcopy(conv2d))
                with torch.backends.mkldnn.flags(enabled=False):
                    y_aten = conv2d(x)
                y_mkldnn = mkldnn_conv2d(x.to_mkldnn()).to_dense()
                self.assertEqual(y_aten, y_mkldnn)

                self._test_serialization(mkldnn_conv2d, (x.to_mkldnn(),))
                self._test_tracing(mkldnn_conv2d, (x.to_mkldnn(),))

    def test_conv2d_legacy_jit_model(self):
        g = 4
        conv2d = torch.nn.Conv2d(16, 16, 3, groups=g)
        conv2d_mkldnn = torch.utils.mkldnn.to_mkldnn(conv2d)

        # contrive legacy conv2d module with a 5-d weight
        o, i, h, w = conv2d.weight.shape
        weight_5d = conv2d.weight.reshape((g, o // g, i, h, w))
        conv2d_mkldnn.weight = weight_5d.to_mkldnn()

        x = torch.randn(1, 16, 8, 8)

        with TemporaryFileName() as fname:
            torch.jit.save(conv2d_mkldnn, fname)
            conv2d_loaded = torch.jit.load(fname)

            self.assertEqual(conv2d_mkldnn.weight.ndimension(), 5)
            self.assertEqual(conv2d_loaded.weight.ndimension(), 4)
            self.assertEqual(
                conv2d(x),
                conv2d_loaded(x.to_mkldnn()).to_dense())

    def test_conv2d_backward(self):
        for groups in [1, 4]:
            N = 64
            C = 3 * groups
            M = 3 * groups
            x = torch.randn(N, C, 224, 224, dtype=torch.float32) * 100
            for bias in [False]:
                conv2d = torch.nn.Conv2d(in_channels=C,
                                         out_channels=M,
                                         kernel_size=3,
                                         stride=2,
                                         padding=1,
                                         bias=bias,
                                         groups=groups).float()
                mkldnn_conv2d = copy.deepcopy(conv2d)
                x1 = x.clone().requires_grad_()
                x2 = x.clone().to_mkldnn().requires_grad_()
                y1 = conv2d(x1).sum()
                y2 = mkldnn_conv2d(x2).to_dense().sum()
                y1.backward()
                y2.backward()
                self.assertEqual(x1.grad, x2.grad.to_dense())
                self.assertEqual(conv2d.weight.grad, mkldnn_conv2d.weight.grad)
                if bias:
                    self.assertEqual(conv2d.bias.grad, mkldnn_conv2d.bias.grad)

    def test_relu(self):
        x = torch.randn((4, 5), dtype=torch.float32) * 10
        self.assertEqual(torch.relu(x), torch.relu(x.to_mkldnn()).to_dense())

    def test_relu_(self):
        x1 = torch.randn((4, 5), dtype=torch.float32) * 10
        x2 = x1.clone().to_mkldnn()
        self.assertEqual(torch.relu_(x1), torch.relu_(x2).to_dense())

    def test_relu_backward(self):
        x = torch.randn((4, 5), dtype=torch.float32) * 10
        x1 = x.clone().requires_grad_()
        x2 = x.clone().to_mkldnn().requires_grad_()
        y1 = torch.relu(x1).sum()
        y2 = torch.relu(x2).to_dense().sum()
        y1.backward()
        y2.backward()
        self.assertEqual(x1.grad, x2.grad.to_dense())
        # inplace
        x1 = x.clone().requires_grad_()
        x2 = x.clone().to_mkldnn().requires_grad_()
        y1 = torch.relu_(x1.clone()).sum()
        y2 = torch.relu_(x2.clone()).to_dense().sum()
        y1.backward()
        y2.backward()
        self.assertEqual(x1.grad, x2.grad.to_dense())

    def test_max_pool2d(self):
        N = torch.randint(3, 10, (1,)).item()
        C = torch.randint(3, 10, (1,)).item()

        for stride in [1, 2, 3]:
            for H, W in [(64, 64), (35, 39), (16, 19), [7, 8]]:
                x = torch.randn(N, C, H, W, dtype=torch.float32) * 10

                for ceil_mode in [False, True]:
                    max_pool2d = torch.nn.MaxPool2d(
                        kernel_size=3 if not ceil_mode else 7,
                        stride=stride,
                        padding=1,
                        ceil_mode=ceil_mode)

                    self.assertEqual(
                        max_pool2d(x),
                        max_pool2d(x.to_mkldnn()).to_dense())

    def test_max_pool2d_backward(self):
        x = torch.randn(10, 3, 64, 64, dtype=torch.float32) * 10
        for ceil_mode in [False, True]:
            max_pool2d = torch.nn.MaxPool2d(
                kernel_size=3,
                stride=2,
                padding=1,
                ceil_mode=ceil_mode)

            x1 = x.clone().requires_grad_()
            x2 = x.clone().to_mkldnn().requires_grad_()

            y1 = max_pool2d(x1).sum()
            y2 = max_pool2d(x2).to_dense().sum()
            y1.backward()
            y2.backward()
            self.assertEqual(x1.grad, x2.grad.to_dense())

    def test_avg_pool2d(self):
        N = torch.randint(3, 10, (1,)).item()
        C = torch.randint(3, 10, (1,)).item()
        x = torch.randn(N, C, 64, 64, dtype=torch.float32) * 10

        for count_include_pad in [True, False]:
            avg_pool2d = torch.nn.AvgPool2d(
                kernel_size=3,
                stride=2,
                padding=1,
                count_include_pad=count_include_pad)

            self.assertEqual(
                avg_pool2d(x),
                avg_pool2d(x.to_mkldnn()).to_dense())

    def test_avg_pool2d_backward(self):
        x = torch.randn(10, 3, 64, 64, dtype=torch.float32) * 10

        for count_include_pad in [True, False]:
            x1 = x.clone().requires_grad_()
            x2 = x.clone().to_mkldnn().requires_grad_()
            avg_pool2d = torch.nn.AvgPool2d(
                kernel_size=3,
                stride=2,
                padding=1,
                count_include_pad=count_include_pad)

            y1 = avg_pool2d(x1).sum()
            y2 = avg_pool2d(x2).to_dense().sum()
            y1.backward()
            y2.backward()
            self.assertEqual(x1.grad, x2.grad.to_dense())

    def test_adaptive_avg_pool2d(self):
        N = torch.randint(3, 10, (1,)).item()
        C = torch.randint(3, 10, (1,)).item()
        x = torch.randn(N, C, 224, 224, dtype=torch.float32) * 100

        adaptive_avg_pool2d = torch.nn.AdaptiveAvgPool2d(7)

        self.assertEqual(
            adaptive_avg_pool2d(x),
            adaptive_avg_pool2d(x.to_mkldnn()).to_dense())

    def test_adaptive_avg_pool2d_backward(self):
        x = torch.randn(10, 3, 224, 224, dtype=torch.float32) * 100

        x1 = x.clone().requires_grad_()
        x2 = x.clone().to_mkldnn().requires_grad_()
        adaptive_avg_pool2d = torch.nn.AdaptiveAvgPool2d(7)

        y1 = adaptive_avg_pool2d(x1).sum()
        y2 = adaptive_avg_pool2d(x2).to_dense().sum()
        y1.backward()
        y2.backward()
        self.assertEqual(x1.grad, x2.grad.to_dense())

    def test_batch_norm2d(self):
        x = torch.randn(64, 3, 35, 45, dtype=torch.float32) * 10

        for train in [True, False]:
            # TODO: support none affine
            for affine in [True]:
                for track_running_stats in [True, False]:
                    bn = torch.nn.BatchNorm2d(
                        3,
                        affine=affine,
                        track_running_stats=track_running_stats).float().train(train)
                    if (train or not track_running_stats):
                        mkldnn_bn = copy.deepcopy(bn)
                    else:
                        mkldnn_bn = mkldnn_utils.to_mkldnn(copy.deepcopy(bn))
                    self.assertEqual(
                        bn(x),
                        mkldnn_bn(x.to_mkldnn()).to_dense())
                    if train and track_running_stats:
                        self.assertEqual(
                            bn.running_mean,
                            mkldnn_bn.running_mean)
                        self.assertEqual(
                            bn.running_var,
                            mkldnn_bn.running_var)
                    if (not train and track_running_stats):
                        self._test_serialization(mkldnn_bn, (x.to_mkldnn(),))
                        self._test_tracing(mkldnn_bn, (x.to_mkldnn(),))

    def test_batch_norm2d_backward(self):
        x = torch.randn(64, 3, 35, 45, dtype=torch.float32) * 10

        # TODO: support none affine
        for affine in [True]:
            for track_running_stats in [True, False]:
                x1 = x.clone().requires_grad_()
                x2 = x.clone().to_mkldnn().requires_grad_()
                bn = torch.nn.BatchNorm2d(
                    3,
                    affine=affine,
                    track_running_stats=track_running_stats).float().train(True)
                mkldnn_bn = copy.deepcopy(bn)
                y1 = bn(x1).sum()
                y2 = mkldnn_bn(x2).to_dense().sum()
                y1.backward()
                y2.backward()
                self.assertEqual(x1.grad, x2.grad.to_dense())

    def test_add(self):
        N = torch.randint(3, 10, (1,)).item()
        C = torch.randint(3, 100, (1,)).item()
        alpha = torch.randn(1, dtype=torch.float32).item()

        x = torch.randn(N, C, 35, 45, dtype=torch.float32) * 10
        y = torch.randn(N, C, 35, 45, dtype=torch.float32) * 10
        mx = x.to_mkldnn()
        my = y.to_mkldnn()

        # add
        self.assertEqual(
            x + y,
            (mx + my).to_dense())

        self.assertEqual(
            torch.add(x, y, alpha=alpha),
            torch.add(mx, my, alpha=alpha).to_dense())

        # add_
        x += y
        mx += my
        self.assertEqual(x, mx.to_dense())

        # add_out
        out = x.clone()
        mkldnn_out = out.to_mkldnn()
        torch.add(x, y, alpha=alpha, out=out)
        torch.add(mx, my, alpha=alpha, out=mkldnn_out)
        self.assertEqual(out, mkldnn_out.to_dense())

    def test_mul(self):
        N = torch.randint(3, 10, (1,)).item()
        C = torch.randint(3, 100, (1,)).item()
        value = torch.randn(1, dtype=torch.float32).item()

        x = torch.randn(N, C, 35, 45, dtype=torch.float32) * 10
        y = torch.randn(N, C, 35, 45, dtype=torch.float32) * 10
        mx = x.to_mkldnn()
        my = y.to_mkldnn()

        # mul
        self.assertEqual(
            x * y,
            (mx * my).to_dense())

        self.assertEqual(
            x * value,
            (mx * value).to_dense())

        self.assertEqual(
            torch.mul(x, y),
            torch.mul(mx, my).to_dense())

        self.assertEqual(
            torch.mul(x, value),
            torch.mul(mx, value).to_dense())

        # mul_
        x *= y
        mx *= my
        self.assertEqual(x, mx.to_dense())

        x *= value
        mx *= value
        self.assertEqual(x, mx.to_dense())

        # mul_out
        out = x.clone()
        mkldnn_out = out.to_mkldnn()
        torch.mul(x, y, out=out)
        torch.mul(mx, my, out=mkldnn_out)
        self.assertEqual(out, mkldnn_out.to_dense())

        out = x.clone()
        mkldnn_out = out.to_mkldnn()
        torch.mul(x, value, out=out)
        torch.mul(mx, value, out=mkldnn_out)
        self.assertEqual(out, mkldnn_out.to_dense())

    def test_view(self):
        x = torch.randn(3, 4, 5, dtype=torch.float32).to_mkldnn()
        self.assertRaisesRegex(RuntimeError,
                               "Change to use reshape",
                               lambda: x.view(x.size(0), -1))

    def test_reshape(self):
        x = torch.randn(3, 4, 5, dtype=torch.float32) * 10
        size = (x.size(0), -1)

        self.assertEqual(
            x.reshape(size),
            x.to_mkldnn().reshape(size).to_dense(),
        )
        # test whether share same memory for plain format tensor
        y = x.to_mkldnn()
        z = y.reshape(size).add_(y.reshape(size))
        self.assertEqual(
            y.reshape(size).to_dense(),
            z.to_dense(),
        )

    def test_reshape_backward(self):
        x = torch.randn(3, 4, 5, dtype=torch.float32) * 10
        size = (x.size(0), -1)

        x1 = x.clone().requires_grad_()
        x2 = x.clone().to_mkldnn().requires_grad_()

        in_features = 20
        out_features = out_features = torch.randint(3, 100, (1,)).item()
        linear = torch.nn.Linear(in_features, out_features).float()

        y1 = linear(x1.reshape(size)).sum()
        y2 = linear(x2.reshape(size).to_dense()).sum()
        y1.backward()
        y2.backward()

        self.assertEqual(
            x1.grad,
            x2.grad.to_dense())

    def test_clone(self):
        x = torch.randn(4, 5, dtype=torch.float32) * 10
        self.assertEqual(
            x.clone(),
            x.to_mkldnn().clone().to_dense(),
        )
        # test whether share same memory
        y = x.to_mkldnn()
        z = y.clone().add_(y)
        self.assertNotEqual(
            y.to_dense(),
            z.to_dense(),
        )

    def test_transpose(self):
        x = torch.randn(3, 4, 5, dtype=torch.float32) * 10
        for dim1 in range(x.ndim):
            for dim2 in range(x.ndim):
                self.assertEqual(
                    x.transpose(dim1, dim2),
                    x.to_mkldnn().transpose(dim1, dim2).to_dense(),
                )

    def test_linear(self):
        in_features = torch.randint(3, 10, (1,)).item()
        out_features = torch.randint(3, 100, (1,)).item()
        x = torch.randn(3, in_features, dtype=torch.float32) * 10

        for bias in [True, False]:
            linear = torch.nn.Linear(in_features, out_features, bias=bias).float()
            mkldnn_linear = mkldnn_utils.to_mkldnn(copy.deepcopy(linear))
            self.assertEqual(
                linear(x),
                mkldnn_linear(x.to_mkldnn()).to_dense())

            self._test_serialization(mkldnn_linear, (x.to_mkldnn(),))
            self._test_tracing(mkldnn_linear, (x.to_mkldnn(),))

    # we should first expose aten::linear, depend on https://github.com/pytorch/pytorch/pull/20039
    def test_linear_backward(self):
        in_features = torch.randint(3, 10, (1,)).item()
        out_features = torch.randint(3, 100, (1,)).item()
        x = torch.randn(3, in_features, dtype=torch.float32) * 10
        for bias in [True, False]:
            x1 = x.clone().requires_grad_()
            x2 = x.clone().to_mkldnn().requires_grad_()
            linear = torch.nn.Linear(in_features, out_features).float()
            mkldnn_linear = copy.deepcopy(linear)
            y1 = linear(x1).sum()
            y2 = mkldnn_linear(x2).to_dense().sum()
            y1.backward()
            y2.backward()
            self.assertEqual(x1.grad, x2.grad.to_dense())
            self.assertEqual(linear.weight.grad, mkldnn_linear.weight.grad)
            if bias:
                self.assertEqual(linear.bias.grad, mkldnn_linear.bias.grad)

    def test_softmax(self):
        x = torch.randn(3, 4, 5, dtype=torch.float32) * 10
        for dim in range(x.ndim):
            softmax = torch.nn.Softmax(dim=dim)
            self.assertEqual(
                softmax(x),
                softmax(x.to_mkldnn()).to_dense())

    def test_sigmoid(self):
        x = torch.randn(4, 5, dtype=torch.float32) * 10
        mkldnn_x = x.to_mkldnn()
        self.assertEqual(
            torch.sigmoid(x),
            torch.sigmoid(mkldnn_x).to_dense(),
        )
        # inplace
        torch.sigmoid_(x)
        torch.sigmoid_(mkldnn_x)
        self.assertEqual(x, mkldnn_x.to_dense())

    def _test_serialization(self, module, inputs):
        with TemporaryFileName() as fname:
            torch.jit.save(module, fname)
            loaded = torch.jit.load(fname)
            self.assertEqual(
                module(*inputs).to_dense(),
                loaded(*inputs).to_dense())

    def _test_tracing(self, module, inputs):
        traced = torch.jit.trace(module, inputs, check_trace=False)
        self.assertEqual(
            module(*inputs).to_dense(),
            traced(*inputs).to_dense())

    def test_set_data_tensorimpl_type(self):
        # Dense tensor has impl of type `TensorImpl`, while MKL-DNN tensor has impl
        # of type `OpaqueTensorImpl<IDeepTensorWrapperPtr>`.
        x = torch.randn((1, 2), dtype=torch.float, device=torch.device('cpu'))
        x_mkldnn = x.to_mkldnn()
        with self.assertRaisesRegex(RuntimeError, 'incompatible tensor type'):
            x.data = x_mkldnn

    def test_empty(self):
        x1 = torch.empty(4, 5, 2, 3, dtype=torch.float32)
        x2 = torch.empty(4, 5, 2, 3, dtype=torch.float32, layout=torch._mkldnn)
        self.assertEqual(x1.size(), x2.to_dense().size())
        self.assertEqual(x1.dtype, x2.to_dense().dtype)

    def test_zero_(self):
        x1 = torch.randn(4, 5, dtype=torch.float32) * 10
        x2 = x1.clone().to_mkldnn()
        self.assertEqual(
            x1.zero_(),
            x2.zero_().to_dense(),
        )

    def test_is_mkldnn(self):
        x = torch.randn(1, dtype=torch.float32)
        self.assertFalse(x.is_mkldnn)
        self.assertTrue(x.to_mkldnn().is_mkldnn)

    # legacy constructor/new doesn't support mkldnn tensors
    def test_legacy_new_failure(self):
        x = torch.randn(1, dtype=torch.float32)
        x_mkldnn = x.to_mkldnn()
        self.assertRaises(RuntimeError, lambda: x_mkldnn.new(device='cpu'))
        self.assertRaises(RuntimeError, lambda: x_mkldnn.new(x.storage()))
        self.assertRaises(RuntimeError, lambda: x_mkldnn.new(x))
        self.assertRaises(RuntimeError, lambda: x_mkldnn.new(torch.Size([2, 3])))
        self.assertRaises(RuntimeError, lambda: x_mkldnn.new([6]))

    def test_is_mkldnn_jit(self):
        class EnsureMkldnn(torch.jit.ScriptModule):
            @torch.jit.script_method
            def forward(self, x):
                if not x.is_mkldnn:
                    x = x.to_mkldnn()
                return x

        m = EnsureMkldnn()
        x = torch.randn(1, dtype=torch.float32)
        self.assertTrue(m(x).is_mkldnn)
        self.assertTrue(m(x.to_mkldnn()).is_mkldnn)

    def _test_imagenet_model(self, model):
        model = model.train(False).float()
        mkldnn_model = mkldnn_utils.to_mkldnn(copy.deepcopy(model))
        x = torch.randn(1, 3, 224, 224, dtype=torch.float32)
        with torch.no_grad():
            self.assertEqual(
                model(x),
                mkldnn_model(x.to_mkldnn()).to_dense(),
            )

    @skipIfNoTorchVision
    def test_resnet18(self):
        model = torchvision.models.resnet.resnet18(pretrained=False)
        self._test_imagenet_model(model)

    @skipIfNoTorchVision
    def test_resnext50_32x4d(self):
        model = torchvision.models.resnet.resnext50_32x4d(pretrained=False)
        self._test_imagenet_model(model)

    def test_cat(self):
        x = torch.randn(4, 5, dtype=torch.float32) * 10
        mkldnn_x = x.to_mkldnn()
        for dim in [0, 1]:
            self.assertEqual(
                torch.cat((x, x, x), dim=dim),
                torch.cat((mkldnn_x, mkldnn_x, mkldnn_x), dim=dim).to_dense(),
            )
        #cat_out
        y = torch.randn(12, 5, dtype=torch.float32)*10
        mkldnn_y = y.to_mkldnn()
        torch.cat((x, x, x), dim=0, out=y),
        torch.cat((mkldnn_x, mkldnn_x, mkldnn_x), dim=0, out=mkldnn_y)
        self.assertEqual(y, mkldnn_y.to_dense())
        y = torch.randn(4, 15, dtype=torch.float32)*10
        mkldnn_y = y.to_mkldnn()
        torch.cat((x, x, x), dim=1, out=y),
        torch.cat((mkldnn_x, mkldnn_x, mkldnn_x), dim=1, out=mkldnn_y)
        self.assertEqual(y, mkldnn_y.to_dense())

    def test_cat_backward(self):
        x = torch.randn((4, 5), dtype=torch.float32) * 10
        x1 = x.clone().requires_grad_()
        x2 = x.clone().to_mkldnn().requires_grad_()
        y1 = torch.cat((x1, x1, x1)).sum()
        y2 = torch.cat((x2, x2, x2)).to_dense().sum()
        y1.backward()
        y2.backward()
        self.assertEqual(x1.grad, x2.grad.to_dense())

    def test_split(self):
        x = torch.randn(5, 5, dtype=torch.float32) * 10
        mkldnn_x = x.to_mkldnn()
        for dim in [0, 1]:
            self.assertEqual(
                torch.split(x, (2,3), dim=dim)[0],
                torch.split(mkldnn_x, (2,3), dim=dim)[0].to_dense(),
            )
            self.assertEqual(
                torch.split(x, (2,3), dim=dim)[1],
                torch.split(mkldnn_x, (2,3), dim=dim)[1].to_dense(),
            )
            self.assertEqual(
                torch.split(x, 3, dim=dim)[0],
                torch.split(mkldnn_x, 3, dim=dim)[0].to_dense(),
            )
            self.assertEqual(
                torch.split(x, 3, dim=dim)[1],
                torch.split(mkldnn_x, 3, dim=dim)[1].to_dense(),
            )
            self.assertEqual(
                torch.split(x, 2, dim=dim)[0],
                torch.split(mkldnn_x, 2, dim=dim)[0].to_dense(),
            )
            self.assertEqual(
                torch.split(x, 2, dim=dim)[1],
                torch.split(mkldnn_x, 2, dim=dim)[1].to_dense(),
            )
            self.assertEqual(
                torch.split(x, 2, dim=dim)[2],
                torch.split(mkldnn_x, 2, dim=dim)[2].to_dense(),
            )

    def test_split_backward(self):
        x = torch.randn(5, 5, dtype=torch.float32) * 10
        x1 = x.clone().requires_grad_()
        x2 = x.clone().to_mkldnn().requires_grad_()
        for dim in [0, 1]:
            y1 = torch.split(x1, (2,3), dim=dim)[0].sum() \
                    + torch.split(x1, (2,3), dim=dim)[1].sum()
            y2 = torch.split(x2, (2,3), dim=dim)[0].to_dense().sum() \
                    + torch.split(x2, (2,3), dim=dim)[1].to_dense().sum()
            y1.backward()
            y2.backward()
            self.assertEqual(x1.grad, x2.grad.to_dense())
            y1 = torch.split(x1, 3, dim=dim)[0].sum() \
                    + torch.split(x1, 3, dim=dim)[1].sum()
            y2 = torch.split(x2, 3, dim=dim)[0].to_dense().sum() \
                    + torch.split(x2, 3, dim=dim)[1].to_dense().sum()
            y1.backward()
            y2.backward()
            self.assertEqual(x1.grad, x2.grad.to_dense())
            y1 = torch.split(x1, 2, dim=dim)[0].sum() \
                    + torch.split(x1, 2, dim=dim)[1].sum() \
                    + torch.split(x1, 2, dim=dim)[2].sum()
            y2 = torch.split(x2, 2, dim=dim)[0].to_dense().sum() \
                    + torch.split(x2, 2, dim=dim)[1].to_dense().sum() \
                    + torch.split(x2, 2, dim=dim)[2].to_dense().sum()
            y1.backward()
            y2.backward()
            self.assertEqual(x1.grad, x2.grad.to_dense())

if __name__ == '__main__':
    run_tests()
