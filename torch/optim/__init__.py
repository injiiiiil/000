"""
:mod:`torch.optim` is a package implementing various optimization algorithms.
Most commonly used methods are already supported, and the interface is general
enough, so that more sophisticated ones can be also easily integrated in the
future.
"""

from .adadelta import Adadelta
from .adagrad import Adagrad
from .adam import Adam
from .adamw import AdamW
from .sparse_adam import SparseAdam
from .adamax import Adamax
from .asgd import ASGD
from .sgd import SGD
from .rprop import Rprop
from .rmsprop import RMSprop
from ._optimizer import Optimizer
from .lbfgs import LBFGS
from . import lr_scheduler
from . import swa_utils


__all__ = ['ASGD', 'Adadelta', 'Adagrad', 'Adam', 'AdamW', 'Adamax',
           'LBFGS', 'Optimizer', 'RMSprop', 'Rprop', 'SGD', 'SparseAdam',
           'lr_scheduler', 'swa_utils']
