import torch
from torch._C import _add_docstr, _special  # type: ignore[attr-defined]
from torch._torch_docs import common_args, multi_dim_common

__all__ = [
    'airy_ai',
    'airy_ai',
    'airy_bi',
    'associated_laguerre_polynomial_l',
    'associated_legendre_p',
    'associated_legendre_q',
    'bell_polynomial_b',
    'bernoulli_number',
    'bernoulli_polynomial_b',
    'bessel_j',
    'bessel_j_0',
    'bessel_j_1',
    'bessel_y',
    'bessel_y_0',
    'bessel_y_1',
    'beta',
    'binomial_coefficient',
    'bose_einstein_integral_g',
    'bulirsch_elliptic_integral_cel',
    'bulirsch_elliptic_integral_el1',
    'bulirsch_elliptic_integral_el2',
    'bulirsch_elliptic_integral_el3',
    'carlson_elliptic_r_c',
    'carlson_elliptic_r_d',
    'carlson_elliptic_r_f',
    'carlson_elliptic_r_g',
    'carlson_elliptic_r_j',
    'chebyshev_polynomial_t',
    'chebyshev_polynomial_u',
    'chebyshev_polynomial_v',
    'chebyshev_polynomial_w',
    'clausen_cl',
    'clausen_sl',
    'complete_carlson_elliptic_r_f',
    'complete_carlson_elliptic_r_g',
    'complete_elliptic_integral_e',
    'complete_elliptic_integral_k',
    'complete_elliptic_integral_pi',
    'complete_legendre_elliptic_integral_d',
    'confluent_hypergeometric_0_f_1',
    'cos_pi',
    'cosh_pi',
    'cosine_integral_ci',
    'debye_d',
    'digamma',
    'dilogarithm_li_2',
    'dirichlet_beta',
    'dirichlet_eta',
    'dirichlet_lambda',
    'double_factorial',
    'entr',
    'erf',
    'erfc',
    'erfcx',
    'erfinv',
    'exp2',
    'exp_airy_ai',
    'exp_airy_bi',
    'exp_modified_bessel_i',
    'exp_modified_bessel_k',
    'exp_modified_bessel_k_0',
    'exp_modified_bessel_k_1',
    'expit',
    'expm1',
    'exponential_integral_e',
    'exponential_integral_ei',
    'factorial',
    'falling_factorial',
    'fermi_dirac_integral_f',
    'fresnel_integral_c',
    'fresnel_integral_s',
    'gammainc',
    'gammaincc',
    'gammaln',
    'gauss_hypergeometric_2_f_1',
    'gegenbauer_polynomial_c',
    'hankel_h_1',
    'hankel_h_2',
    'harmonic_number',
    'hermite_polynomial_h',
    'hermite_polynomial_he',
    'heuman_lambda',
    'hurwitz_zeta',
    'hyperbolic_cosine_integral_chi',
    'hyperbolic_sine_integral_shi',
    'i0',
    'i0e',
    'i1',
    'i1e',
    'incomplete_beta',
    'incomplete_elliptic_integral_e',
    'incomplete_elliptic_integral_f',
    'incomplete_elliptic_integral_pi',
    'incomplete_legendre_elliptic_integral_d',
    'jacobi_polynomial_p',
    'jacobi_theta_1',
    'jacobi_theta_2',
    'jacobi_theta_3',
    'jacobi_theta_4',
    'jacobi_zeta',
    'kummer_confluent_hypergeometric_1_f_1',
    'laguerre_polynomial_l',
    'lah_number',
    'legendre_polynomial_p',
    'legendre_q',
    'ln_binomial_coefficient',
    'ln_double_factorial',
    'ln_factorial',
    'ln_falling_factorial',
    'ln_gamma',
    'ln_gamma_sign',
    'ln_rising_factorial',
    'log1p',
    'log_ndtr',
    'log_softmax',
    'logarithmic_integral_li',
    'logit',
    'logsumexp',
    'lower_incomplete_gamma',
    'modified_bessel_i',
    'modified_bessel_i_0',
    'modified_bessel_i_1',
    'modified_bessel_k',
    'modified_bessel_k_0',
    'modified_bessel_k_1',
    'multigammaln',
    'ndtr',
    'ndtri',
    'neville_theta_c',
    'neville_theta_d',
    'neville_theta_n',
    'neville_theta_s',
    'nome_q',
    'owens_t',
    'polar_pi',
    'polygamma',
    'polylogarithm_li',
    'prime_number',
    'psi',
    'radial_polynomial_r',
    'reciprocal_gamma',
    'riemann_zeta',
    'rising_factorial',
    'round',
    'shifted_chebyshev_polynomial_t',
    'shifted_chebyshev_polynomial_u',
    'shifted_chebyshev_polynomial_v',
    'shifted_chebyshev_polynomial_w',
    'sin_pi',
    'sinc',
    'sinc_pi',
    'sinh_pi',
    'sinhc',
    'sinhc_pi',
    'softmax',
    'spherical_bessel_j',
    'spherical_bessel_j_0',
    'spherical_bessel_y',
    'spherical_hankel_h_1',
    'spherical_hankel_h_2',
    'spherical_harmonic_y',
    'spherical_legendre_y',
    'spherical_modified_bessel_i',
    'spherical_modified_bessel_k',
    'stirling_number_1',
    'stirling_number_2',
    'tan_pi',
    'tanh_pi',
    'theta_1',
    'theta_2',
    'theta_3',
    'theta_4',
    'tricomi_confluent_hypergeometric_u',
    'upper_incomplete_gamma',
    'xlog1py',
    'xlogy',
    'zernike_polynomial_z',
    'zeta'
]

Tensor = torch.Tensor

entr = _add_docstr(_special.special_entr,
                   r"""
entr(input, *, out=None) -> Tensor
Computes the entropy on :attr:`input` (as defined below), elementwise.

.. math::
    \begin{align}
    \text{entr(x)} = \begin{cases}
        -x * \ln(x)  & x > 0 \\
        0 &  x = 0.0 \\
        -\infty & x < 0
    \end{cases}
    \end{align}
""" + """

Args:
   input (Tensor): the input tensor.

Keyword args:
    out (Tensor, optional): the output tensor.

Example::
    >>> a = torch.arange(-0.5, 1, 0.5)
    >>> a
    tensor([-0.5000,  0.0000,  0.5000])
    >>> torch.special.entr(a)
    tensor([  -inf, 0.0000, 0.3466])
""")

psi = _add_docstr(_special.special_psi,
                  r"""
psi(input, *, out=None) -> Tensor

Alias for :func:`torch.special.digamma`.
""")

digamma = _add_docstr(_special.special_digamma,
                      r"""
digamma(input, *, out=None) -> Tensor

Computes the logarithmic derivative of the gamma function on `input`.

.. math::
    \digamma(x) = \frac{d}{dx} \ln\left(\Gamma\left(x\right)\right) = \frac{\Gamma'(x)}{\Gamma(x)}
""" + r"""
Args:
    input (Tensor): the tensor to compute the digamma function on

Keyword args:
    {out}

.. note::  This function is similar to SciPy's `scipy.special.digamma`.

.. note::  From PyTorch 1.8 onwards, the digamma function returns `-Inf` for `0`.
           Previously it returned `NaN` for `0`.

Example::

    >>> a = torch.tensor([1, 0.5])
    >>> torch.special.digamma(a)
    tensor([-0.5772, -1.9635])

""".format(**common_args))

gammaln = _add_docstr(_special.special_gammaln,
                      r"""
gammaln(input, *, out=None) -> Tensor

Computes the natural logarithm of the absolute value of the gamma function on :attr:`input`.

.. math::
    \text{out}_{i} = \ln \Gamma(|\text{input}_{i}|)
""" + """
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> a = torch.arange(0.5, 2, 0.5)
    >>> torch.special.gammaln(a)
    tensor([ 0.5724,  0.0000, -0.1208])

""".format(**common_args))

polygamma = _add_docstr(_special.special_polygamma,
                        r"""
polygamma(n, input, *, out=None) -> Tensor

Computes the :math:`n^{th}` derivative of the digamma function on :attr:`input`.
:math:`n \geq 0` is called the order of the polygamma function.

.. math::
    \psi^{(n)}(x) = \frac{d^{(n)}}{dx^{(n)}} \psi(x)

.. note::
    This function is implemented only for nonnegative integers :math:`n \geq 0`.
""" + """
Args:
    n (int): the order of the polygamma function
    {input}

Keyword args:
    {out}

Example::
    >>> a = torch.tensor([1, 0.5])
    >>> torch.special.polygamma(1, a)
    tensor([1.64493, 4.9348])
    >>> torch.special.polygamma(2, a)
    tensor([ -2.4041, -16.8288])
    >>> torch.special.polygamma(3, a)
    tensor([ 6.4939, 97.4091])
    >>> torch.special.polygamma(4, a)
    tensor([ -24.8863, -771.4742])
""".format(**common_args))

erf = _add_docstr(_special.special_erf,
                  r"""
erf(input, *, out=None) -> Tensor

Computes the error function of :attr:`input`. The error function is defined as follows:

.. math::
    \mathrm{erf}(x) = \frac{2}{\sqrt{\pi}} \int_{0}^{x} e^{-t^2} dt
""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> torch.special.erf(torch.tensor([0, -1., 10.]))
    tensor([ 0.0000, -0.8427,  1.0000])
""".format(**common_args))

erfc = _add_docstr(_special.special_erfc,
                   r"""
erfc(input, *, out=None) -> Tensor

Computes the complementary error function of :attr:`input`.
The complementary error function is defined as follows:

.. math::
    \mathrm{erfc}(x) = 1 - \frac{2}{\sqrt{\pi}} \int_{0}^{x} e^{-t^2} dt
""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> torch.special.erfc(torch.tensor([0, -1., 10.]))
    tensor([ 1.0000, 1.8427,  0.0000])
""".format(**common_args))

erfcx = _add_docstr(_special.special_erfcx,
                    r"""
erfcx(input, *, out=None) -> Tensor

Computes the scaled complementary error function for each element of :attr:`input`.
The scaled complementary error function is defined as follows:

.. math::
    \mathrm{erfcx}(x) = e^{x^2} \mathrm{erfc}(x)
""" + r"""

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> torch.special.erfcx(torch.tensor([0, -1., 10.]))
    tensor([ 1.0000, 5.0090, 0.0561])
""".format(**common_args))

erfinv = _add_docstr(_special.special_erfinv,
                     r"""
erfinv(input, *, out=None) -> Tensor

Computes the inverse error function of :attr:`input`.
The inverse error function is defined in the range :math:`(-1, 1)` as:

.. math::
    \mathrm{erfinv}(\mathrm{erf}(x)) = x
""" + r"""

Args:
    {input}

Keyword args:
    {out}

Example::

    >>> torch.special.erfinv(torch.tensor([0, 0.5, -1.]))
    tensor([ 0.0000,  0.4769,    -inf])
""".format(**common_args))

logit = _add_docstr(_special.special_logit,
                    r"""
logit(input, eps=None, *, out=None) -> Tensor

Returns a new tensor with the logit of the elements of :attr:`input`.
:attr:`input` is clamped to [eps, 1 - eps] when eps is not None.
When eps is None and :attr:`input` < 0 or :attr:`input` > 1, the function will yields NaN.

.. math::
    \begin{align}
    y_{i} &= \ln(\frac{z_{i}}{1 - z_{i}}) \\
    z_{i} &= \begin{cases}
        x_{i} & \text{if eps is None} \\
        \text{eps} & \text{if } x_{i} < \text{eps} \\
        x_{i} & \text{if } \text{eps} \leq x_{i} \leq 1 - \text{eps} \\
        1 - \text{eps} & \text{if } x_{i} > 1 - \text{eps}
    \end{cases}
    \end{align}
""" + r"""
Args:
    {input}
    eps (float, optional): the epsilon for input clamp bound. Default: ``None``

Keyword args:
    {out}

Example::

    >>> a = torch.rand(5)
    >>> a
    tensor([0.2796, 0.9331, 0.6486, 0.1523, 0.6516])
    >>> torch.special.logit(a, eps=1e-6)
    tensor([-0.9466,  2.6352,  0.6131, -1.7169,  0.6261])
""".format(**common_args))

logsumexp = _add_docstr(_special.special_logsumexp,
                        r"""
logsumexp(input, dim, keepdim=False, *, out=None)

Alias for :func:`torch.logsumexp`.
""".format(**multi_dim_common))

expit = _add_docstr(_special.special_expit,
                    r"""
expit(input, *, out=None) -> Tensor

Computes the expit (also known as the logistic sigmoid function) of the elements of :attr:`input`.

.. math::
    \text{out}_{i} = \frac{1}{1 + e^{-\text{input}_{i}}}
""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> t = torch.randn(4)
    >>> t
    tensor([ 0.9213,  1.0887, -0.8858, -1.7683])
    >>> torch.special.expit(t)
    tensor([ 0.7153,  0.7481,  0.2920,  0.1458])
""".format(**common_args))

exp2 = _add_docstr(_special.special_exp2,
                   r"""
exp2(input, *, out=None) -> Tensor

Computes the base two exponential function of :attr:`input`.

.. math::
    y_{i} = 2^{x_{i}}

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> torch.special.exp2(torch.tensor([0, math.log2(2.), 3, 4]))
    tensor([ 1.,  2.,  8., 16.])
""".format(**common_args))

expm1 = _add_docstr(_special.special_expm1,
                    r"""
expm1(input, *, out=None) -> Tensor

Computes the exponential of the elements minus 1
of :attr:`input`.

.. math::
    y_{i} = e^{x_{i}} - 1

.. note:: This function provides greater precision than exp(x) - 1 for small values of x.

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::

    >>> torch.special.expm1(torch.tensor([0, math.log(2.)]))
    tensor([ 0.,  1.])
""".format(**common_args))

xlog1py = _add_docstr(_special.special_xlog1py,
                      r"""
xlog1py(input, other, *, out=None) -> Tensor

Computes ``input * log1p(other)`` with the following cases.

.. math::
    \text{out}_{i} = \begin{cases}
        \text{NaN} & \text{if } \text{other}_{i} = \text{NaN} \\
        0 & \text{if } \text{input}_{i} = 0.0 \text{ and } \text{other}_{i} != \text{NaN} \\
        \text{input}_{i} * \text{log1p}(\text{other}_{i})& \text{otherwise}
    \end{cases}

Similar to SciPy's `scipy.special.xlog1py`.

""" + r"""

Args:
    input (Number or Tensor) : Multiplier
    other (Number or Tensor) : Argument

.. note:: At least one of :attr:`input` or :attr:`other` must be a tensor.

Keyword args:
    {out}

Example::

    >>> x = torch.zeros(5,)
    >>> y = torch.tensor([-1, 0, 1, float('inf'), float('nan')])
    >>> torch.special.xlog1py(x, y)
    tensor([0., 0., 0., 0., nan])
    >>> x = torch.tensor([1, 2, 3])
    >>> y = torch.tensor([3, 2, 1])
    >>> torch.special.xlog1py(x, y)
    tensor([1.3863, 2.1972, 2.0794])
    >>> torch.special.xlog1py(x, 4)
    tensor([1.6094, 3.2189, 4.8283])
    >>> torch.special.xlog1py(2, y)
    tensor([2.7726, 2.1972, 1.3863])
""".format(**common_args))

xlogy = _add_docstr(_special.special_xlogy,
                    r"""
xlogy(input, other, *, out=None) -> Tensor

Computes ``input * log(other)`` with the following cases.

.. math::
    \text{out}_{i} = \begin{cases}
        \text{NaN} & \text{if } \text{other}_{i} = \text{NaN} \\
        0 & \text{if } \text{input}_{i} = 0.0 \\
        \text{input}_{i} * \log{(\text{other}_{i})} & \text{otherwise}
    \end{cases}

Similar to SciPy's `scipy.special.xlogy`.

""" + r"""

Args:
    input (Number or Tensor) : Multiplier
    other (Number or Tensor) : Argument

.. note:: At least one of :attr:`input` or :attr:`other` must be a tensor.

Keyword args:
    {out}

Example::

    >>> x = torch.zeros(5,)
    >>> y = torch.tensor([-1, 0, 1, float('inf'), float('nan')])
    >>> torch.special.xlogy(x, y)
    tensor([0., 0., 0., 0., nan])
    >>> x = torch.tensor([1, 2, 3])
    >>> y = torch.tensor([3, 2, 1])
    >>> torch.special.xlogy(x, y)
    tensor([1.0986, 1.3863, 0.0000])
    >>> torch.special.xlogy(x, 4)
    tensor([1.3863, 2.7726, 4.1589])
    >>> torch.special.xlogy(2, y)
    tensor([2.1972, 1.3863, 0.0000])
""".format(**common_args))

i0 = _add_docstr(_special.special_i0,
                 r"""
i0(input, *, out=None) -> Tensor

Computes the zeroth order modified Bessel function of the first kind for each element of :attr:`input`.

.. math::
    \text{out}_{i} = I_0(\text{input}_{i}) = \sum_{k=0}^{\infty} \frac{(\text{input}_{i}^2/4)^k}{(k!)^2}

""" + r"""
Args:
    input (Tensor): the input tensor

Keyword args:
    {out}

Example::

    >>> torch.i0(torch.arange(5, dtype=torch.float32))
    tensor([ 1.0000,  1.2661,  2.2796,  4.8808, 11.3019])

""".format(**common_args))

i0e = _add_docstr(_special.special_i0e,
                  r"""
i0e(input, *, out=None) -> Tensor
Computes the exponentially scaled zeroth order modified Bessel function of the first kind (as defined below)
for each element of :attr:`input`.

.. math::
    \text{out}_{i} = \exp(-|x|) * i0(x) = \exp(-|x|) * \sum_{k=0}^{\infty} \frac{(\text{input}_{i}^2/4)^k}{(k!)^2}

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::
    >>> torch.special.i0e(torch.arange(5, dtype=torch.float32))
    tensor([1.0000, 0.4658, 0.3085, 0.2430, 0.2070])
""".format(**common_args))

i1 = _add_docstr(_special.special_i1,
                 r"""
i1(input, *, out=None) -> Tensor
Computes the first order modified Bessel function of the first kind (as defined below)
for each element of :attr:`input`.

.. math::
    \text{out}_{i} = \frac{(\text{input}_{i})}{2} * \sum_{k=0}^{\infty} \frac{(\text{input}_{i}^2/4)^k}{(k!) * (k+1)!}

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::
    >>> torch.special.i1(torch.arange(5, dtype=torch.float32))
    tensor([0.0000, 0.5652, 1.5906, 3.9534, 9.7595])
""".format(**common_args))

i1e = _add_docstr(_special.special_i1e,
                  r"""
i1e(input, *, out=None) -> Tensor
Computes the exponentially scaled first order modified Bessel function of the first kind (as defined below)
for each element of :attr:`input`.

.. math::
    \text{out}_{i} = \exp(-|x|) * i1(x) =
        \exp(-|x|) * \frac{(\text{input}_{i})}{2} * \sum_{k=0}^{\infty} \frac{(\text{input}_{i}^2/4)^k}{(k!) * (k+1)!}

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::
    >>> torch.special.i1e(torch.arange(5, dtype=torch.float32))
    tensor([0.0000, 0.2079, 0.2153, 0.1968, 0.1788])
""".format(**common_args))

ndtr = _add_docstr(_special.special_ndtr,
                   r"""
ndtr(input, *, out=None) -> Tensor
Computes the area under the standard Gaussian probability density function,
integrated from minus infinity to :attr:`input`, elementwise.

.. math::
    \text{ndtr}(x) = \frac{1}{\sqrt{2 \pi}}\int_{-\infty}^{x} e^{-\frac{1}{2}t^2} dt

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::
    >>> torch.special.ndtr(torch.tensor([-3., -2, -1, 0, 1, 2, 3]))
    tensor([0.0013, 0.0228, 0.1587, 0.5000, 0.8413, 0.9772, 0.9987])
""".format(**common_args))

ndtri = _add_docstr(_special.special_ndtri,
                    r"""
ndtri(input, *, out=None) -> Tensor
Computes the argument, x, for which the area under the Gaussian probability density function
(integrated from minus infinity to x) is equal to :attr:`input`, elementwise.

.. math::
    \text{ndtri}(p) = \sqrt{2}\text{erf}^{-1}(2p - 1)

.. note::
    Also known as quantile function for Normal Distribution.

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::
    >>> torch.special.ndtri(torch.tensor([0, 0.25, 0.5, 0.75, 1]))
    tensor([   -inf, -0.6745,  0.0000,  0.6745,     inf])
""".format(**common_args))

log_ndtr = _add_docstr(_special.special_log_ndtr,
                       r"""
log_ndtr(input, *, out=None) -> Tensor
Computes the log of the area under the standard Gaussian probability density function,
integrated from minus infinity to :attr:`input`, elementwise.

.. math::
    \text{log\_ndtr}(x) = \log\left(\frac{1}{\sqrt{2 \pi}}\int_{-\infty}^{x} e^{-\frac{1}{2}t^2} dt \right)

""" + r"""
Args:
    {input}

Keyword args:
    {out}

Example::
    >>> torch.special.log_ndtr(torch.tensor([-3., -2, -1, 0, 1, 2, 3]))
    tensor([-6.6077 -3.7832 -1.841  -0.6931 -0.1728 -0.023  -0.0014])
""".format(**common_args))

log1p = _add_docstr(_special.special_log1p,
                    r"""
log1p(input, *, out=None) -> Tensor

Alias for :func:`torch.log1p`.
""")

sinc = _add_docstr(_special.special_sinc,
                   r"""
sinc(input, *, out=None) -> Tensor

Computes the normalized sinc of :attr:`input.`

.. math::
    \text{out}_{i} =
    \begin{cases}
      1, & \text{if}\ \text{input}_{i}=0 \\
      \sin(\pi \text{input}_{i}) / (\pi \text{input}_{i}), & \text{otherwise}
    \end{cases}
""" + r"""

Args:
    {input}

Keyword args:
    {out}

Example::
    >>> t = torch.randn(4)
    >>> t
    tensor([ 0.2252, -0.2948,  1.0267, -1.1566])
    >>> torch.special.sinc(t)
    tensor([ 0.9186,  0.8631, -0.0259, -0.1300])
""".format(**common_args))

round = _add_docstr(_special.special_round,
                    r"""
round(input, *, out=None) -> Tensor

Alias for :func:`torch.round`.
""")

softmax = _add_docstr(_special.special_softmax,
                      r"""
softmax(input, dim, *, dtype=None) -> Tensor

Computes the softmax function.

Softmax is defined as:

:math:`\text{Softmax}(x_{i}) = \frac{\exp(x_i)}{\sum_j \exp(x_j)}`

It is applied to all slices along dim, and will re-scale them so that the elements
lie in the range `[0, 1]` and sum to 1.

Args:
    input (Tensor): input
    dim (int): A dimension along which softmax will be computed.
    dtype (:class:`torch.dtype`, optional): the desired data type of returned tensor.
        If specified, the input tensor is cast to :attr:`dtype` before the operation
        is performed. This is useful for preventing data type overflows. Default: None.

Examples::
    >>> t = torch.ones(2, 2)
    >>> torch.special.softmax(t, 0)
    tensor([[0.5000, 0.5000],
            [0.5000, 0.5000]])

""")

log_softmax = _add_docstr(_special.special_log_softmax,
                          r"""
log_softmax(input, dim, *, dtype=None) -> Tensor

Computes softmax followed by a logarithm.

While mathematically equivalent to log(softmax(x)), doing these two
operations separately is slower and numerically unstable. This function
is computed as:

.. math::
    \text{log\_softmax}(x_{i}) = \log\left(\frac{\exp(x_i) }{ \sum_j \exp(x_j)} \right)
""" + r"""

Args:
    input (Tensor): input
    dim (int): A dimension along which log_softmax will be computed.
    dtype (:class:`torch.dtype`, optional): the desired data type of returned tensor.
        If specified, the input tensor is cast to :attr:`dtype` before the operation
        is performed. This is useful for preventing data type overflows. Default: None.

Example::
    >>> t = torch.ones(2, 2)
    >>> torch.special.log_softmax(t, 0)
    tensor([[-0.6931, -0.6931],
            [-0.6931, -0.6931]])
""")

zeta = _add_docstr(_special.special_zeta,
                   r"""
zeta(input, other, *, out=None) -> Tensor

Computes the Hurwitz zeta function, elementwise.

.. math::
    \zeta(x, q) = \sum_{k=0}^{\infty} \frac{1}{(k + q)^x}

""" + r"""
Args:
    input (Tensor): the input tensor corresponding to `x`.
    other (Tensor): the input tensor corresponding to `q`.

.. note::
    The Riemann zeta function corresponds to the case when `q = 1`

Keyword args:
    {out}

Example::
    >>> x = torch.tensor([2., 4.])
    >>> torch.special.zeta(x, 1)
    tensor([1.6449, 1.0823])
    >>> torch.special.zeta(x, torch.tensor([1., 2.]))
    tensor([1.6449, 0.0823])
    >>> torch.special.zeta(2, torch.tensor([1., 2.]))
    tensor([1.6449, 0.6449])
""".format(**common_args))

multigammaln = _add_docstr(_special.special_multigammaln,
                           r"""
multigammaln(input, p, *, out=None) -> Tensor

Computes the `multivariate log-gamma function
<https://en.wikipedia.org/wiki/Multivariate_gamma_function>`_ with dimension
:math:`p` element-wise, given by

.. math::
    \log(\Gamma_{p}(a)) = C + \displaystyle \sum_{i=1}^{p} \log\left(\Gamma\left(a - \frac{i - 1}{2}\right)\right)

where :math:`C = \log(\pi) \times \frac{p (p - 1)}{4}` and :math:`\Gamma(\cdot)` is the Gamma function.

All elements must be greater than :math:`\frac{p - 1}{2}`, otherwise an error would be thrown.
""" + """

Args:
    input (Tensor): the tensor to compute the multivariate log-gamma function
    p (int): the number of dimensions

Keyword args:
    {out}

Example::

    >>> a = torch.empty(2, 3).uniform_(1, 2)
    >>> a
    tensor([[1.6835, 1.8474, 1.1929],
            [1.0475, 1.7162, 1.4180]])
    >>> torch.special.multigammaln(a, 2)
    tensor([[0.3928, 0.4007, 0.7586],
            [1.0311, 0.3901, 0.5049]])
""".format(**common_args))

gammainc = _add_docstr(_special.special_gammainc,
                       r"""
gammainc(input, other, *, out=None) -> Tensor

Computes the regularized lower incomplete gamma function:

.. math::
    \text{out}_{i} = \frac{1}{\Gamma(\text{input}_i)} \int_0^{\text{other}_i} t^{\text{input}_i-1} e^{-t} dt

where both :math:`\text{input}_i` and :math:`\text{other}_i` are weakly positive
and at least one is strictly positive.
If both are zero or either is negative then :math:`\text{out}_i=\text{nan}`.
:math:`\Gamma(\cdot)` in the equation above is the gamma function,

.. math::
    \Gamma(\text{input}_i) = \int_0^\infty t^{(\text{input}_i-1)} e^{-t} dt.

See :func:`torch.special.gammaincc` and :func:`torch.special.gammaln` for related functions.

Supports :ref:`broadcasting to a common shape <broadcasting-semantics>`
and float inputs.

.. note::
    The backward pass with respect to :attr:`input` is not yet supported.
    Please open an issue on PyTorch's Github to request it.

""" + r"""
Args:
    input (Tensor): the first non-negative input tensor
    other (Tensor): the second non-negative input tensor

Keyword args:
    {out}

Example::

    >>> a1 = torch.tensor([4.0])
    >>> a2 = torch.tensor([3.0, 4.0, 5.0])
    >>> a = torch.special.gammaincc(a1, a2)
    tensor([0.3528, 0.5665, 0.7350])
    tensor([0.3528, 0.5665, 0.7350])
    >>> b = torch.special.gammainc(a1, a2) + torch.special.gammaincc(a1, a2)
    tensor([1., 1., 1.])

""".format(**common_args))

gammaincc = _add_docstr(_special.special_gammaincc,
                        r"""
gammaincc(input, other, *, out=None) -> Tensor

Computes the regularized upper incomplete gamma function:

.. math::
    \text{out}_{i} = \frac{1}{\Gamma(\text{input}_i)} \int_{\text{other}_i}^{\infty} t^{\text{input}_i-1} e^{-t} dt

where both :math:`\text{input}_i` and :math:`\text{other}_i` are weakly positive
and at least one is strictly positive.
If both are zero or either is negative then :math:`\text{out}_i=\text{nan}`.
:math:`\Gamma(\cdot)` in the equation above is the gamma function,

.. math::
    \Gamma(\text{input}_i) = \int_0^\infty t^{(\text{input}_i-1)} e^{-t} dt.

See :func:`torch.special.gammainc` and :func:`torch.special.gammaln` for related functions.

Supports :ref:`broadcasting to a common shape <broadcasting-semantics>`
and float inputs.

.. note::
    The backward pass with respect to :attr:`input` is not yet supported.
    Please open an issue on PyTorch's Github to request it.

""" + r"""
Args:
    input (Tensor): the first non-negative input tensor
    other (Tensor): the second non-negative input tensor

Keyword args:
    {out}

Example::

    >>> a1 = torch.tensor([4.0])
    >>> a2 = torch.tensor([3.0, 4.0, 5.0])
    >>> a = torch.special.gammaincc(a1, a2)
    tensor([0.6472, 0.4335, 0.2650])
    >>> b = torch.special.gammainc(a1, a2) + torch.special.gammaincc(a1, a2)
    tensor([1., 1., 1.])

""".format(**common_args))

airy_ai = _add_docstr(
    _special.special_airy_ai,
    r"""
airy_ai(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

airy_bi = _add_docstr(
    _special.special_airy_bi,
    r"""
airy_bi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

associated_laguerre_polynomial_l = _add_docstr(
    _special.special_associated_laguerre_polynomial_l,
    r"""
associated_laguerre_polynomial_l(n, m, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    m (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

associated_legendre_p = _add_docstr(
    _special.special_associated_legendre_p,
    r"""
associated_legendre_p(l, m, x, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    m (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

associated_legendre_q = _add_docstr(
    _special.special_associated_legendre_q,
    r"""
associated_legendre_q(l, m, x, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    m (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bell_polynomial_b = _add_docstr(
    _special.special_bell_polynomial_b,
    r"""
bell_polynomial_b(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bernoulli_number = _add_docstr(
    _special.special_bernoulli_number,
    r"""
bernoulli_number(n, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bernoulli_polynomial_b = _add_docstr(
    _special.special_bernoulli_polynomial_b,
    r"""
bernoulli_polynomial_b(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bessel_j = _add_docstr(
    _special.special_bessel_j,
    r"""
bessel_j(v, z, *, out=None) -> Tensor

    """ + r"""
Args:
    v (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bessel_j_0 = _add_docstr(
    _special.special_bessel_j_0,
    r"""
bessel_j_0(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bessel_j_1 = _add_docstr(
    _special.special_bessel_j_1,
    r"""
bessel_j_1(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bessel_y = _add_docstr(
    _special.special_bessel_y,
    r"""
bessel_y(v, z, *, out=None) -> Tensor

    """ + r"""
Args:
    v (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bessel_y_0 = _add_docstr(
    _special.special_bessel_y_0,
    r"""
bessel_y_0(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bessel_y_1 = _add_docstr(
    _special.special_bessel_y_1,
    r"""
bessel_y_1(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

beta = _add_docstr(
    _special.special_beta,
    r"""
beta(a, b, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    b (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

binomial_coefficient = _add_docstr(
    _special.special_binomial_coefficient,
    r"""
binomial_coefficient(n, k, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    k (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bose_einstein_integral_g = _add_docstr(
    _special.special_bose_einstein_integral_g,
    r"""
bose_einstein_integral_g(s, x, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bulirsch_elliptic_integral_cel = _add_docstr(
    _special.special_bulirsch_elliptic_integral_cel,
    r"""
bulirsch_elliptic_integral_cel(k, p, a, b, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    p (Scalar or Tensor):
    a (Scalar or Tensor):
    b (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bulirsch_elliptic_integral_el1 = _add_docstr(
    _special.special_bulirsch_elliptic_integral_el1,
    r"""
bulirsch_elliptic_integral_el1(x, k, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bulirsch_elliptic_integral_el2 = _add_docstr(
    _special.special_bulirsch_elliptic_integral_el2,
    r"""
bulirsch_elliptic_integral_el2(x, k, a, b, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    k (Scalar or Tensor):
    a (Scalar or Tensor):
    b (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

bulirsch_elliptic_integral_el3 = _add_docstr(
    _special.special_bulirsch_elliptic_integral_el3,
    r"""
bulirsch_elliptic_integral_el3(x, k, p, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    k (Scalar or Tensor):
    p (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

carlson_elliptic_r_c = _add_docstr(
    _special.special_carlson_elliptic_r_c,
    r"""
carlson_elliptic_r_c(x, y, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

carlson_elliptic_r_d = _add_docstr(
    _special.special_carlson_elliptic_r_d,
    r"""
carlson_elliptic_r_d(x, y, z, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

carlson_elliptic_r_f = _add_docstr(
    _special.special_carlson_elliptic_r_f,
    r"""
carlson_elliptic_r_f(x, y, z, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

carlson_elliptic_r_g = _add_docstr(
    _special.special_carlson_elliptic_r_g,
    r"""
carlson_elliptic_r_g(x, y, z, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

carlson_elliptic_r_j = _add_docstr(
    _special.special_carlson_elliptic_r_j,
    r"""
carlson_elliptic_r_j(x, y, z, p, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):
    z (Scalar or Tensor):
    p (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

chebyshev_polynomial_t = _add_docstr(
    _special.special_chebyshev_polynomial_t,
    r"""
chebyshev_polynomial_t(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

chebyshev_polynomial_u = _add_docstr(
    _special.special_chebyshev_polynomial_u,
    r"""
chebyshev_polynomial_u(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

chebyshev_polynomial_v = _add_docstr(
    _special.special_chebyshev_polynomial_v,
    r"""
chebyshev_polynomial_v(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

chebyshev_polynomial_w = _add_docstr(
    _special.special_chebyshev_polynomial_w,
    r"""
chebyshev_polynomial_w(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

clausen_cl = _add_docstr(
    _special.special_clausen_cl,
    r"""
clausen_cl(m, x, *, out=None) -> Tensor

    """ + r"""
Args:
    m (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

clausen_sl = _add_docstr(
    _special.special_clausen_sl,
    r"""
clausen_sl(m, x, *, out=None) -> Tensor

    """ + r"""
Args:
    m (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

complete_carlson_elliptic_r_f = _add_docstr(
    _special.special_complete_carlson_elliptic_r_f,
    r"""
complete_carlson_elliptic_r_f(x, y, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

complete_carlson_elliptic_r_g = _add_docstr(
    _special.special_complete_carlson_elliptic_r_g,
    r"""
complete_carlson_elliptic_r_g(x, y, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Scalar or Tensor):
    y (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

complete_elliptic_integral_e = _add_docstr(
    _special.special_complete_elliptic_integral_e,
    r"""
complete_elliptic_integral_e(k, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

complete_elliptic_integral_k = _add_docstr(
    _special.special_complete_elliptic_integral_k,
    r"""
complete_elliptic_integral_k(k, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

complete_elliptic_integral_pi = _add_docstr(
    _special.special_complete_elliptic_integral_pi,
    r"""
complete_elliptic_integral_pi(k, n, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    n (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

complete_legendre_elliptic_integral_d = _add_docstr(
    _special.special_complete_legendre_elliptic_integral_d,
    r"""
complete_legendre_elliptic_integral_d(k, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

confluent_hypergeometric_0_f_1 = _add_docstr(
    _special.special_confluent_hypergeometric_0_f_1,
    r"""
confluent_hypergeometric_0_f_1(c, x, *, out=None) -> Tensor

    """ + r"""
Args:
    c (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

cos_pi = _add_docstr(
    _special.special_cos_pi,
    r"""
cos_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

cosh_pi = _add_docstr(
    _special.special_cosh_pi,
    r"""
cosh_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

cosine_integral_ci = _add_docstr(
    _special.special_cosine_integral_ci,
    r"""
cosine_integral_ci(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

debye_d = _add_docstr(
    _special.special_debye_d,
    r"""
debye_d(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

dilogarithm_li_2 = _add_docstr(
    _special.special_dilogarithm_li_2,
    r"""
dilogarithm_li_2(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

dirichlet_beta = _add_docstr(
    _special.special_dirichlet_beta,
    r"""
dirichlet_beta(s, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

dirichlet_eta = _add_docstr(
    _special.special_dirichlet_eta,
    r"""
dirichlet_eta(s, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

dirichlet_lambda = _add_docstr(
    _special.special_dirichlet_lambda,
    r"""
dirichlet_eta(s, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

double_factorial = _add_docstr(
    _special.special_double_factorial,
    r"""
double_factorial(n, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exp_airy_ai = _add_docstr(
    _special.special_exp_airy_ai,
    r"""
exp_airy_ai(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exp_airy_bi = _add_docstr(
    _special.special_exp_airy_bi,
    r"""
exp_airy_bi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exp_modified_bessel_i = _add_docstr(
    _special.special_exp_modified_bessel_i,
    r"""
exp_modified_bessel_i(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exp_modified_bessel_k = _add_docstr(
    _special.special_exp_modified_bessel_k,
    r"""
exp_modified_bessel_k(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exp_modified_bessel_k_0 = _add_docstr(
    _special.special_exp_modified_bessel_k_0,
    r"""
exp_modified_bessel_k_0(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exp_modified_bessel_k_1 = _add_docstr(
    _special.special_exp_modified_bessel_k_1,
    r"""
exp_modified_bessel_k_1(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exponential_integral_e = _add_docstr(
    _special.special_exponential_integral_e,
    r"""
exponential_integral_e(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

exponential_integral_ei = _add_docstr(
    _special.special_exponential_integral_ei,
    r"""
exponential_integral_ei(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

factorial = _add_docstr(
    _special.special_factorial,
    r"""
factorial(n, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

falling_factorial = _add_docstr(
    _special.special_falling_factorial,
    r"""
falling_factorial(a, n, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    n (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

fermi_dirac_integral_f = _add_docstr(
    _special.special_fermi_dirac_integral_f,
    r"""
fermi_dirac_integral_f(s, x, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

fresnel_integral_c = _add_docstr(
    _special.special_fresnel_integral_c,
    r"""
fresnel_integral_c(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

fresnel_integral_s = _add_docstr(
    _special.special_fresnel_integral_s,
    r"""
fresnel_integral_s(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

gauss_hypergeometric_2_f_1 = _add_docstr(
    _special.special_gauss_hypergeometric_2_f_1,
    r"""
gauss_hypergeometric_2_f_1(a, b, c, x, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    b (Scalar or Tensor):
    c (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

gegenbauer_polynomial_c = _add_docstr(
    _special.special_gegenbauer_polynomial_c,
    r"""
gegenbauer_polynomial_c(l, n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hankel_h_1 = _add_docstr(
    _special.special_hankel_h_1,
    r"""
hankel_h_1(v, z, *, out=None) -> Tensor

    """ + r"""
Args:
    v (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hankel_h_2 = _add_docstr(
    _special.special_hankel_h_2,
    r"""
hankel_h_2(v, z, *, out=None) -> Tensor

    """ + r"""
Args:
    v (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

harmonic_number = _add_docstr(
    _special.special_harmonic_number,
    r"""
harmonic_number(n, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hermite_polynomial_h = _add_docstr(
    _special.special_hermite_polynomial_h,
    r"""
hermite_polynomial_h(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hermite_polynomial_he = _add_docstr(
    _special.special_hermite_polynomial_he,
    r"""
hermite_polynomial_he(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

heuman_lambda = _add_docstr(
    _special.special_heuman_lambda,
    r"""
heuman_lambda(k, p, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    p (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hurwitz_zeta = _add_docstr(
    _special.special_hurwitz_zeta,
    r"""
hurwitz_zeta(s, a, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Scalar or Tensor):
    a (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hyperbolic_cosine_integral_chi = _add_docstr(
    _special.special_hyperbolic_cosine_integral_chi,
    r"""
hyperbolic_cosine_integral_chi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

hyperbolic_sine_integral_shi = _add_docstr(
    _special.special_hyperbolic_sine_integral_shi,
    r"""
hyperbolic_sine_integral_shi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

incomplete_beta = _add_docstr(
    _special.special_incomplete_beta,
    r"""
incomplete_beta(a, b, x, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    b (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

incomplete_elliptic_integral_e = _add_docstr(
    _special.special_incomplete_elliptic_integral_e,
    r"""
incomplete_elliptic_integral_e(k, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

incomplete_elliptic_integral_f = _add_docstr(
    _special.special_incomplete_elliptic_integral_f,
    r"""
incomplete_elliptic_integral_f(k, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

incomplete_elliptic_integral_pi = _add_docstr(
    _special.special_incomplete_elliptic_integral_pi,
    r"""
incomplete_elliptic_integral_pi(k, n, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    n (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

incomplete_legendre_elliptic_integral_d = _add_docstr(
    _special.special_incomplete_legendre_elliptic_integral_d,
    r"""
incomplete_legendre_elliptic_integral_d(k, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

jacobi_polynomial_p = _add_docstr(
    _special.special_jacobi_polynomial_p,
    r"""
jacobi_polynomial_p(alpha, beta, n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    alpha (Scalar or Tensor):
    beta (Scalar or Tensor):
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

jacobi_theta_1 = _add_docstr(
    _special.special_jacobi_theta_1,
    r"""
jacobi_theta_1(q, x, *, out=None) -> Tensor

    """ + r"""
Args:
    q (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

jacobi_theta_2 = _add_docstr(
    _special.special_jacobi_theta_2,
    r"""
jacobi_theta_2(q, x, *, out=None) -> Tensor

    """ + r"""
Args:
    q (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

jacobi_theta_3 = _add_docstr(
    _special.special_jacobi_theta_3,
    r"""
jacobi_theta_3(q, x, *, out=None) -> Tensor

    """ + r"""
Args:
    q (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

jacobi_theta_4 = _add_docstr(
    _special.special_jacobi_theta_4,
    r"""
jacobi_theta_4(q, x, *, out=None) -> Tensor

    """ + r"""
Args:
    q (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

jacobi_zeta = _add_docstr(
    _special.special_jacobi_zeta,
    r"""
jacobi_zeta(k, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

kummer_confluent_hypergeometric_1_f_1 = _add_docstr(
    _special.special_kummer_confluent_hypergeometric_1_f_1,
    r"""
kummer_confluent_hypergeometric_1_f_1(a, c, x, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    c (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

laguerre_polynomial_l = _add_docstr(
    _special.special_laguerre_polynomial_l,
    r"""
laguerre_polynomial_l(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

lah_number = _add_docstr(
    _special.special_lah_number,
    r"""
lah_number(n, k, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    k (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

legendre_polynomial_p = _add_docstr(
    _special.special_legendre_polynomial_p,
    r"""
legendre_polynomial_p(l, x, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

legendre_q = _add_docstr(
    _special.special_legendre_q,
    r"""
legendre_q(l, x, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_binomial_coefficient = _add_docstr(
    _special.special_ln_binomial_coefficient,
    r"""
ln_binomial_coefficient(n, k, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    k (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_double_factorial = _add_docstr(
    _special.special_ln_double_factorial,
    r"""
ln_double_factorial(n, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_factorial = _add_docstr(
    _special.special_ln_factorial,
    r"""
ln_gamma(n, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_falling_factorial = _add_docstr(
    _special.special_ln_falling_factorial,
    r"""
ln_falling_factorial(a, n, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    n (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_gamma = _add_docstr(
    _special.special_ln_gamma,
    r"""
ln_gamma(a, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_gamma_sign = _add_docstr(
    _special.special_ln_gamma_sign,
    r"""
ln_gamma_sign(a, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

ln_rising_factorial = _add_docstr(
    _special.special_ln_rising_factorial,
    r"""
ln_rising_factorial(a, n, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    n (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

logarithmic_integral_li = _add_docstr(
    _special.special_logarithmic_integral_li,
    r"""
logarithmic_integral_li(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

lower_incomplete_gamma = _add_docstr(
    _special.special_lower_incomplete_gamma,
    r"""
lower_incomplete_gamma(a, z, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

modified_bessel_i = _add_docstr(
    _special.special_modified_bessel_i,
    r"""
modified_bessel_k(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

modified_bessel_i_0 = _add_docstr(
    _special.special_modified_bessel_i_0,
    r"""
modified_bessel_i_0(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

modified_bessel_i_1 = _add_docstr(
    _special.special_modified_bessel_i_1,
    r"""
modified_bessel_i_1(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

modified_bessel_k = _add_docstr(
    _special.special_modified_bessel_k,
    r"""
modified_bessel_k(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

modified_bessel_k_0 = _add_docstr(
    _special.special_modified_bessel_k_0,
    r"""
modified_bessel_k_0(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

modified_bessel_k_1 = _add_docstr(
    _special.special_modified_bessel_k_1,
    r"""
modified_bessel_k_1(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

neville_theta_c = _add_docstr(
    _special.special_neville_theta_c,
    r"""
neville_theta_c(k, x, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

neville_theta_d = _add_docstr(
    _special.special_neville_theta_d,
    r"""
neville_theta_d(k, x, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

neville_theta_n = _add_docstr(
    _special.special_neville_theta_n,
    r"""
neville_theta_n(k, x, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

neville_theta_s = _add_docstr(
    _special.special_neville_theta_s,
    r"""
neville_theta_s(k, x, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

nome_q = _add_docstr(
    _special.special_nome_q,
    r"""
nome_q(k, *, out=None) -> Tensor

    """ + r"""
Args:
    k (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

owens_t = _add_docstr(
    _special.special_owens_t,
    r"""
owens_t(h, a, *, out=None) -> Tensor

    """ + r"""
Args:
    h (Scalar or Tensor):
    a (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

polar_pi = _add_docstr(
    _special.special_polar_pi,
    r"""
polar_pi(rho, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    rho (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

polylogarithm_li = _add_docstr(
    _special.special_polylogarithm_li,
    r"""
polylogarithm_li(s, x, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

prime_number = _add_docstr(
    _special.special_prime_number,
    r"""
prime_number(n, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

radial_polynomial_r = _add_docstr(
    _special.special_radial_polynomial_r,
    r"""
radial_polynomial_r(m, n, rho, *, out=None) -> Tensor

    """ + r"""
Args:
    m (Scalar or Tensor):
    n (Scalar or Tensor):
    rho (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

reciprocal_gamma = _add_docstr(
    _special.special_reciprocal_gamma,
    r"""
reciprocal_gamma(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

riemann_zeta = _add_docstr(
    _special.special_riemann_zeta,
    r"""
riemann_zeta(s, *, out=None) -> Tensor

    """ + r"""
Args:
    s (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

rising_factorial = _add_docstr(
    _special.special_rising_factorial,
    r"""
rising_factorial(a, n, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    n (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

shifted_chebyshev_polynomial_t = _add_docstr(
    _special.special_shifted_chebyshev_polynomial_t,
    r"""
shifted_chebyshev_polynomial_t(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

shifted_chebyshev_polynomial_u = _add_docstr(
    _special.special_shifted_chebyshev_polynomial_u,
    r"""
shifted_chebyshev_polynomial_u(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

shifted_chebyshev_polynomial_v = _add_docstr(
    _special.special_shifted_chebyshev_polynomial_v,
    r"""
shifted_chebyshev_polynomial_v(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

shifted_chebyshev_polynomial_w = _add_docstr(
    _special.special_shifted_chebyshev_polynomial_w,
    r"""
shifted_chebyshev_polynomial_w(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

sin_pi = _add_docstr(
    _special.special_sin_pi,
    r"""
sin_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

sinc_pi = _add_docstr(
    _special.special_sinc_pi,
    r"""
sinc_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

sinh_pi = _add_docstr(
    _special.special_sinh_pi,
    r"""
sinh_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

sinhc = _add_docstr(
    _special.special_sinhc,
    r"""
sinhc(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

sinhc_pi = _add_docstr(
    _special.special_sinhc_pi,
    r"""
sinhc_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_bessel_j = _add_docstr(
    _special.special_spherical_bessel_j,
    r"""
spherical_bessel_j(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_bessel_j_0 = _add_docstr(
    _special.special_spherical_bessel_j_0,
    r"""
spherical_bessel_j_0(x, *, out=None) -> Tensor

    """ + r"""
Args:
    x (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_bessel_y = _add_docstr(
    _special.special_spherical_bessel_y,
    r"""
spherical_bessel_y(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_hankel_h_1 = _add_docstr(
    _special.special_spherical_hankel_h_1,
    r"""
spherical_hankel_h_1(n, z, *, out=None) -> Tensor
    """ + r"""
Args:
    n (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_hankel_h_2 = _add_docstr(
    _special.special_spherical_hankel_h_2,
    r"""
spherical_hankel_h_2(n, z, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_harmonic_y = _add_docstr(
    _special.special_spherical_harmonic_y,
    r"""
spherical_harmonic_y(l, m, t, p, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    m (Scalar or Tensor):
    t (Scalar or Tensor):
    p (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_legendre_y = _add_docstr(
    _special.special_spherical_legendre_y,
    r"""
spherical_legendre_y(l, m, theta, *, out=None) -> Tensor

    """ + r"""
Args:
    l (Scalar or Tensor):
    m (Scalar or Tensor):
    theta (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_modified_bessel_i = _add_docstr(
    _special.special_spherical_modified_bessel_i,
    r"""
spherical_modified_bessel_i(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

spherical_modified_bessel_k = _add_docstr(
    _special.special_spherical_modified_bessel_k,
    r"""
spherical_modified_bessel_k(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

stirling_number_1 = _add_docstr(
    _special.special_stirling_number_1,
    r"""
stirling_number_1(n, m, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    m (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

stirling_number_2 = _add_docstr(
    _special.special_stirling_number_2,
    r"""
stirling_number_2(n, m, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    m (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

tan_pi = _add_docstr(
    _special.special_tan_pi,
    r"""
tan_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

tanh_pi = _add_docstr(
    _special.special_tanh_pi,
    r"""
tanh_pi(z, *, out=None) -> Tensor

    """ + r"""
Args:
    z (Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

theta_1 = _add_docstr(
    _special.special_theta_1,
    r"""
theta_1(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

theta_2 = _add_docstr(
    _special.special_theta_2,
    r"""
theta_2(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

theta_3 = _add_docstr(
    _special.special_theta_3,
    r"""
theta_3(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

theta_4 = _add_docstr(
    _special.special_theta_4,
    r"""
theta_4(n, x, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

tricomi_confluent_hypergeometric_u = _add_docstr(
    _special.special_tricomi_confluent_hypergeometric_u,
    r"""
tricomi_confluent_hypergeometric_u(a, c, x, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    c (Scalar or Tensor):
    x (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

upper_incomplete_gamma = _add_docstr(
    _special.special_upper_incomplete_gamma,
    r"""
upper_incomplete_gamma(a, z, *, out=None) -> Tensor

    """ + r"""
Args:
    a (Scalar or Tensor):
    z (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)

zernike_polynomial_z = _add_docstr(
    _special.special_zernike_polynomial_z,
    r"""
zernike_polynomial_z(n, m, rho, phi, *, out=None) -> Tensor

    """ + r"""
Args:
    n (Scalar or Tensor):
    m (Scalar or Tensor):
    rho (Scalar or Tensor):
    phi (Scalar or Tensor):

Keyword args:
    {out}
    """.format(**common_args),
)
