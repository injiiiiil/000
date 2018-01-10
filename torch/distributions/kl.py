import warnings
from functools import total_ordering

import torch
import math

from .distribution import Distribution
from .bernoulli import Bernoulli
from .beta import Beta
from .exponential import Exponential
from .gamma import Gamma
from .laplace import Laplace
from .normal import Normal
from .pareto import Pareto
from .uniform import Uniform

_KL_REGISTRY = {}  # Source of truth mapping a few general (type, type) pairs to functions.
_KL_MEMOIZE = {}  # Memoized version mapping many specific (type, type) pairs to functions.


def register_kl(type_p, type_q):
    """
    Decorator to register a pairwise function with :meth:`kl_divergence`.
    Usage::

        @register_kl(Normal, Normal)
        def kl_normal_normal(p, q):
            # insert implementation here

    Lookup returns the most specific (type,type) match ordered by subclass. If
    the match is ambiguous, a `RuntimeWarning` is raised. For example to
    resolve the ambiguous situation::

        @register_kl(BaseP, DerivedQ)
        def kl_version1(p, q): ...
        @register_kl(DerivedP, BaseQ)
        def kl_version2(p, q): ...

    you should register a third most-specific implementation, e.g.::

        register_kl(DerivedP, DerivedQ)(kl_version1)  # Break the tie.

    Args:
        type_p (type): A subclass of :class:`~torch.distributions.Distribution`.
        type_q (type): A subclass of :class:`~torch.distributions.Distribution`.
    """
    if not isinstance(type_p, type) and issubclass(type_p, Distribution):
        raise TypeError('Expected type_p to be a Distribution subclass but got {}'.format(type_p))
    if not isinstance(type_q, type) and issubclass(type_q, Distribution):
        raise TypeError('Expected type_q to be a Distribution subclass but got {}'.format(type_q))

    def decorator(fun):
        _KL_REGISTRY[type_p, type_q] = fun
        _KL_MEMOIZE.clear()  # reset since lookup order may have changed
        return fun

    return decorator


@total_ordering
class _Match(object):
    __slots__ = ['types']

    def __init__(self, *types):
        self.types = types

    def __eq__(self, other):
        return self.types == other.types

    def __le__(self, other):
        for x, y in zip(self.types, other.types):
            if not issubclass(x, y):
                return False
            if x is not y:
                break
        return True


def _dispatch_kl(type_p, type_q):
    """
    Find the most specific approximate match, assuming single inheritance.
    """
    matches = [(super_p, super_q) for super_p, super_q in _KL_REGISTRY
               if issubclass(type_p, super_p) and issubclass(type_q, super_q)]
    if not matches:
        return NotImplemented
    # Check that the left- and right- lexicographic orders agree.
    left_p, left_q = min(_Match(*m) for m in matches).types
    right_q, right_p = min(_Match(*reversed(m)) for m in matches).types
    left_fun = _KL_REGISTRY[left_p, left_q]
    right_fun = _KL_REGISTRY[right_p, right_q]
    if left_fun is not right_fun:
        warnings.warn('Ambiguous kl_divergence({}, {}). Please register_kl({}, {})'.format(
            type_p.__name__, type_q.__name__, left_p.__name__, right_q.__name__),
            RuntimeWarning)
    return left_fun


def _infinite_like(tensor):
    """
    Helper function for obtaining infinite KL Divergence throughout
    """
    return tensor.new([float('inf')]).expand_as(tensor)


def kl_divergence(p, q):
    r"""
    Compute Kullback-Leibler divergence :math:`KL(p \| q)` between two distributions.

    .. math::

        KL(p \| q) = \int p(x) \log\frac {p(x)} {q(x)} \,dx

    Args:
        p (Distrubution): A :class:`~torch.distributions.Distribution` object.
        q (Distrubution): A :class:`~torch.distributions.Distribution` object.

    Returns:
        Variable or Tensor: A batch of KL divergences of shape `batch_shape`.

    Raises:
        NotImplementedError: If the distribution types have not been registered via
            :meth:`register_kl`.
    """
    try:
        fun = _KL_MEMOIZE[type(p), type(q)]
    except KeyError:
        fun = _dispatch_kl(type(p), type(q))
        _KL_MEMOIZE[type(p), type(q)] = fun
    if fun is NotImplemented:
        raise NotImplementedError
    return fun(p, q)


################################################################################
# KL Divergence Implementations
################################################################################

# Same distributions

@register_kl(Bernoulli, Bernoulli)
def _kl_bernoulli_bernoulli(p, q):
    t1 = p.probs * (p.probs / q.probs).log()
    t2 = (1 - p.probs) * ((1 - p.probs) / (1 - q.probs)).log()
    return t1 + t2

@register_kl(Beta, Beta)
def _kl_beta_beta(p, q):
    sum_params_p = p.alpha + p.beta
    sum_params_q = q.alpha + q.beta
    t1 = q.alpha.lgamma() + q.beta.lgamma() + (sum_params_p).lgamma()
    t2 = p.alpha.lgamma() + p.beta.lgamma() + (sum_params_q).lgamma()
    t3 = (p.alpha - q.alpha) * torch.digamma(p.alpha)
    t4 = (p.beta - q.beta) * torch.digamma(p.beta)
    t5 = (sum_params_q - sum_params_p) * torch.digamma(sum_params_p)
    return t1 - t2 + t3 + t4 + t5

@register_kl(Exponential, Exponential)
def _kl_exponential_exponential(p, q):
    rate_ratio = q.rate / p.rate
    t1 = -rate_ratio.log()
    return t1 + rate_ratio - 1

@register_kl(Gamma, Gamma)
def _kl_gamma_gamma(p, q):
    t1 = q.alpha * (p.beta / q.beta).log()
    t2 = torch.lgamma(q.alpha) - torch.lgamma(p.alpha)
    t3 = (p.alpha - q.alpha) * torch.digamma(p.alpha)
    t4 = (q.beta - p.beta) * (p.alpha / p.beta)
    return t1 + t2 + t3 + t4

@register_kl(Laplace, Laplace)
def _kl_laplace_laplace(p, q):
    #  From http://www.mast.queensu.ca/~communications/Papers/gil-msc11.pdf
    scale_ratio = p.scale / q.scale
    loc_abs_diff = (p.loc - q.loc).abs()
    t1 = -scale_ratio.log()
    t2 = loc_abs_diff / q.scale
    t3 = scale_ratio * torch.exp(-loc_abs_diff / p.scale)
    return t1 + t2 + t3 - 1

@register_kl(Normal, Normal)
def _kl_normal_normal(p, q):
    std_dev_ratio = p.std / q.std
    t1 = -std_dev_ratio.log()
    t2 = std_dev_ratio.pow(2)
    t3 = ((p.mean - q.mean) / q.std).pow(2)
    return t1 + (t2 + t3 - 1) / 2

@register_kl(Pareto, Pareto)
def _kl_pareto_pareto(p, q):
    #  From http://www.mast.queensu.ca/~communications/Papers/gil-msc11.pdf
    scale_ratio = p.scale / q.scale
    alpha_ratio = q.alpha / p.alpha
    t1 = q.alpha * scale_ratio.log()
    t2 = -alpha_ratio.log()
    result = t1 + t2 + alpha_ratio - 1
    result[p.support.lower_bound < q.support.lower_bound] = float('inf')
    return result

@register_kl(Uniform, Uniform)
def _kl_uniform_uniform(p, q):
    result = ((q.high - q.low) / (p.high - p.low)).log()
    result[(q.low > p.low) | (q.high < p.high)] = float('inf')
    return result

# Different distributions

@register_kl(Beta, Pareto)
def _kl_beta_infinity(p, q):
    return _infinite_like(p.alpha)

@register_kl(Beta, Exponential)
def _kl_beta_exponential(p, q):
    return -p.entropy() - q.rate.log() + q.rate * (p.alpha / (p.alpha + p.beta))

@register_kl(Beta, Gamma)
def _kl_beta_gamma(p, q):
    t1 = -p.entropy()
    t2 = q.alpha.lgamma() - q.alpha * q.beta.log()
    t3 = (q.alpha - 1) * (p.alpha.digamma() - (p.alpha + p.beta).digamma())
    t4 = q.beta * p.alpha / (p.alpha + p.beta)
    return t1 + t2 - t3 + t4

@register_kl(Beta, Normal)
def _kl_beta_normal(p, q):
    E_beta = p.alpha / (p.alpha + p.beta)
    var_normal = q.std.pow(2)
    t1 = -p.entropy()
    t2 = 0.5 * (var_normal * 2 * math.pi).log()
    t3 = (E_beta * (1 - E_beta) / (p.alpha + p.beta + 1) + E_beta.pow(2)) * 0.5
    t4 = q.mean * E_beta
    t5 = q.mean.pow(2) * 0.5
    return t1 + t2 + (t3 - t4 + t5) / var_normal

@register_kl(Beta, Uniform)
def _kl_beta_uniform(p, q):
    result = -p.entropy() + (q.high - q.low).log()
    result[(q.low > p.support.lower_bound) | (q.high < p.support.upper_bound)] = float('inf')
    return result

@register_kl(Exponential, Beta)
@register_kl(Exponential, Pareto)
@register_kl(Exponential, Uniform)
def _kl_exponential_infinity(p, q):
    return _infinite_like(p.rate)

@register_kl(Exponential, Gamma)
def _kl_exponential_gamma(p, q):
    ratio = q.beta / p.rate
    t1 = -q.alpha * torch.log(ratio)
    return t1 + ratio + q.alpha.lgamma() + q.alpha * 0.57721566490153286060 - 1.57721566490153286060

@register_kl(Exponential, Normal)
def _kl_exponential_normal(p, q):
    var_normal = q.std.pow(2)
    rate_sqr = p.rate.pow(2)
    t1 = 0.5 * torch.log(rate_sqr * var_normal * 2 * math.pi)
    t2 = rate_sqr.reciprocal()
    t3 = q.mean / p.rate
    t4 = q.mean.pow(2) * 0.5
    return t1 - 1 + (t2 - t3 + t4) / var_normal

@register_kl(Gamma, Beta)
@register_kl(Gamma, Pareto)
@register_kl(Gamma, Uniform)
def _kl_gamma_infinity(p, q):
    return _infinite_like(p.alpha)

@register_kl(Gamma, Exponential)
def _kl_gamma_exponential(p, q):
    return -p.entropy() - q.rate.log() + q.rate * p.alpha / p.beta

@register_kl(Gamma, Normal)
def _kl_gamma_exponential(p, q):
    var_normal = q.std.pow(2)
    beta_sqr = p.beta.pow(2)
    t1 = 0.5 * torch.log(beta_sqr * var_normal * 2 * math.pi) - p.alpha - p.alpha.lgamma()
    t2 = 0.5 * (p.alpha.pow(2) + p.alpha) / beta_sqr
    t3 = q.mean * p.alpha / p.beta
    t4 = 0.5 * q.mean.pow(2)
    return t1 + (p.alpha - 1) * p.alpha.digamma() + (t2 - t3 + t4) / var_normal

@register_kl(Laplace, Beta)
@register_kl(Laplace, Exponential)
@register_kl(Laplace, Gamma)
@register_kl(Laplace, Pareto)
@register_kl(Laplace, Uniform)
def _kl_laplace_infinity(p, q):
    return _infinite_like(p.loc)

@register_kl(Laplace, Normal)
def _kl_laplace_normal(p, q):
    var_normal = q.std.pow(2)
    scale_sqr_var_ratio = p.scale.pow(2) / var_normal
    t1 = 0.5 * torch.log(2 * scale_sqr_var_ratio / math.pi)
    t2 = 0.5 * p.loc.pow(2)
    t3 = p.loc * q.mean
    t4 = 0.5 * q.mean.pow(2)
    return -t1 + scale_sqr_var_ratio + (t2 - t3 + t4) / var_normal - 1

@register_kl(Normal, Beta)
@register_kl(Normal, Exponential)
@register_kl(Normal, Gamma)
@register_kl(Normal, Pareto)
@register_kl(Normal, Uniform)
def _kl_normal_infinity(p, q):
    return torch.new(p.mean.size()).fill_(float('inf'))

@register_kl(Normal, Laplace)
def _kl_normal_laplace(p, q):
    common_term = (p.std / q.scale)
    common_const = math.sqrt(2.0 / math.pi)
    return (math.log(common_const) - 0.5) - torch.log(common_term) + common_term * common_const

@register_kl(Pareto, Beta)
@register_kl(Pareto, Uniform)
def _kl_pareto_infinity(p, q):
    return _infinite_like(p.scale)

@register_kl(Pareto, Exponential)
def _kl_pareto_exponential(p, q):
    param_prod = p.alpha * q.rate
    t1 = torch.log(param_prod / p.scale)
    t2 = param_prod * p.scale / (p.alpha - 1)
    result = -t1 + t2 - p.alpha.reciprocal() - 1
    result[(p.support.lower_bound < q.support.lower_bound) | p.alpha <= 1] = float('inf')
    return result

@register_kl(Pareto, Gamma)
def _kl_pareto_gamma(p, q):
    t1 = torch.log(p.scale.pow(p.alpha) * q.beta.pow(q.alpha) / p.alpha)
    t2 = q.beta * p.alpha * p.scale / (p.alpha - 1)
    result = -2 - t1 + q.alpha.lgamma() + t2
    result[(p.support.lower < q.support.lower_bound) | p.alpha <= 1] = float('inf')
    return result

@register_kl(Pareto, Normal)
def _kl_pareto_normal(p, q):
    var_normal = q.std.pow(2)
    common_term = p.scale / (p.alpha - 1)
    alpha_sqr = p.alpha.pow(2)
    t1 = p.alpha.reciprocal()
    t2 = 0.5 * (p.alpha.pow(2) * 2 * math.pi * var_normal / p.scale.pow(2)).log()
    t3 = 0.5 * common_term.pow(2) * (p.alpha / p.alpha - 2 + alpha_sqr)
    t3[p.alpha <= 2] = float('inf')
    t4 = q.mean * common_term * p.alpha
    t5 = q.mean.pow(2)
    return -t1 + t2 + (t3 - t4 + t5) / var_normal - 1

@register_kl(Uniform, Beta)
def _kl_uniform_beta(p, q):
    common_term = p.high - p.low
    t1 = torch.log(common_term)
    def ret_x_log_x(tensor):
        return x * x.log()
    t2 = (q.alpha - 1) * (ret_x_log_x(p.high) - ret_x_log_x(p.low) - common_term) / common_term
    t3 = (q.beta - 1) * (ret_x_log_x((1 - p.high)) - ret_x_log_x((1 - p.low)) + common_term) / common_term
    t4 = q.alpha.lgamma() + q.beta.lgamma() - (q.alpha + q.beta).lgamma()
    result = -t1 -t2 + t3 + t4
    result[(p.high > q.support.upper_bound) | (p.low < q.support.lower_bound)] = float('inf')
    return result
