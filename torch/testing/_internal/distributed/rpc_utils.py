#!/usr/bin/env python3
import unittest
from enum import Flag, auto
from typing import Dict, List, NamedTuple, Type

from torch.testing._internal.common_distributed import MultiProcessTestCase
from torch.testing._internal.common_utils import TEST_WITH_ASAN, TEST_WITH_TSAN
from torch.testing._internal.distributed.ddp_under_dist_autograd_test import (
    DdpComparisonTest,
    DdpUnderDistAutogradTest,
)
from torch.testing._internal.distributed.nn.api.remote_module_test import (
    RemoteModuleTest,
)
from torch.testing._internal.distributed.rpc.dist_autograd_test import (
    DistAutogradTest,
    FaultyAgentDistAutogradTest,
)
from torch.testing._internal.distributed.rpc.dist_optimizer_test import (
    DistOptimizerTest,
)
from torch.testing._internal.distributed.rpc.jit.dist_autograd_test import (
    JitDistAutogradTest,
)
from torch.testing._internal.distributed.rpc.jit.rpc_test import JitRpcTest
from torch.testing._internal.distributed.rpc.jit.rpc_test_faulty import (
    JitFaultyAgentRpcTest,
)
from torch.testing._internal.distributed.rpc.rpc_agent_test_fixture import (
    RpcAgentTestFixture,
)
from torch.testing._internal.distributed.rpc.rpc_test import (
    FaultyAgentRpcTest,
    ProcessGroupAgentRpcTest,
    RpcTest,
    TensorPipeAgentRpcTest,
)


# The tests for the RPC module need to cover multiple possible combinations:
# - different aspects of the API, each one having its own suite of tests;
# - different agents (ProcessGroup, TensorPipe, ...);
# - and subprocesses launched with either fork or spawn.
# To avoid a combinatorial explosion in code size, and to prevent forgetting to
# add a combination, these are generated automatically by the code in this file.
# Here, we collect all the test suites that we need to cover and the two multi-
# processing methods. We then have one separate file for each agent, from which
# we call the generate_tests function of this file, passing to it a fixture for
# the agent, which then gets mixed-in with each test suite and each mp method.


@unittest.skipIf(TEST_WITH_TSAN, "TSAN and fork() is broken")
class ForkHelper(MultiProcessTestCase):
    def setUp(self):
        super().setUp()
        self._fork_processes()


@unittest.skipIf(
    TEST_WITH_ASAN, "Skip ASAN as torch + multiprocessing spawn have known issues"
)
class SpawnHelper(MultiProcessTestCase):
    def setUp(self):
        super().setUp()
        self._spawn_processes()


class MultiProcess(Flag):
    FORK = auto()
    SPAWN = auto()


MP_HELPERS_AND_SUFFIXES = {
    MultiProcess.FORK: (ForkHelper, "WithFork"),
    MultiProcess.SPAWN: (SpawnHelper, "WithSpawn"),
}


class Test(NamedTuple):
    test_class: Type[RpcAgentTestFixture]
    mp_type: MultiProcess


# This list contains test suites that are agent-agnostic and that only verify
# compliance with the generic RPC interface specification. These tests should
# *not* make use of implementation details of a specific agent (options,
# attributes, ...). These test suites will be instantiated multiple times, once
# for each agent (except the faulty agent, which is special).
GENERIC_TESTS = [
    Test(RpcTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(DistAutogradTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(DistOptimizerTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(JitRpcTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(JitDistAutogradTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(RemoteModuleTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(DdpUnderDistAutogradTest, MultiProcess.SPAWN),
    Test(DdpComparisonTest, MultiProcess.SPAWN),
]


# This list contains test suites that will only be run on the ProcessGroupAgent.
# These suites should be standalone, and separate from the ones in the generic
# list (not subclasses of those!).
PROCESS_GROUP_TESTS = [
    Test(ProcessGroupAgentRpcTest, MultiProcess.FORK | MultiProcess.SPAWN)
]


# This list contains test suites that will only be run on the TensorPipeAgent.
# These suites should be standalone, and separate from the ones in the generic
# list (not subclasses of those!).
TENSORPIPE_TESTS = [
    Test(TensorPipeAgentRpcTest, MultiProcess.FORK | MultiProcess.SPAWN)
]


# This list contains test suites that will only be run on the faulty RPC agent.
# That agent is special as it's only used to perform fault injection in order to
# verify the error handling behavior. Thus the faulty agent will only run the
# suites in this list, which were designed to test such behaviors, and not the
# ones in the generic list.
FAULTY_AGENT_TESTS = [
    Test(FaultyAgentRpcTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(FaultyAgentDistAutogradTest, MultiProcess.FORK | MultiProcess.SPAWN),
    Test(JitFaultyAgentRpcTest, MultiProcess.FORK | MultiProcess.SPAWN),
]


def generate_tests(
    prefix: str,
    mixin: Type[RpcAgentTestFixture],
    tests: List[Test],
    mp_type_filter: MultiProcess,
    module_name: str,
) -> Dict[str, Type[RpcAgentTestFixture]]:
    """Mix in the classes needed to autogenerate the tests based on the params.

    Takes a series of test suites, each written against a "generic" agent (i.e.,
    derived from the abstract RpcAgentTestFixture class), as the `tests` args.
    Takes a concrete subclass of RpcAgentTestFixture, which specializes it for a
    certain agent, as the `mixin` arg. Produces all combinations of them, and of
    the multiprocessing start methods (fork or spawn), possibly filtered using
    the `mp_type_filter`. Returns a dictionary of class names to class type
    objects which can be inserted into the global namespace of the calling
    module. The name of each test will be a concatenation of the `prefix` arg,
    the original name of the test suite, and a suffix of either `WithFork` or
    `WithSpawn`. The `module_name` should be the name of the calling module so
    that the classes can be fixed to make it look like they belong to it, which
    is necessary for pickling to work on them.
    """
    ret: Dict[str, Type[RpcAgentTestFixture]] = {}
    for test in tests:
        for mp_type in MultiProcess:
            if mp_type & mp_type_filter & test.mp_type:
                mp_helper, suffix = MP_HELPERS_AND_SUFFIXES[mp_type]
                name = f"{prefix}{test.test_class.__name__}{suffix}"
                class_ = type(name, (test.test_class, mixin, mp_helper), dict())
                class_.__module__ = module_name
                ret[name] = class_
    return ret
