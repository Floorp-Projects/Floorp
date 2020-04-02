#!/usr/bin/env python
import mozunit
from mozperftest.base import MachEnvironment, MultipleMachEnvironment
from mock import MagicMock


class Env(MachEnvironment):
    called = 0

    def setup(self):
        self.called += 1

    def teardown(self):
        self.called += 1


def test_mach_environement():
    mach = MagicMock()

    with Env(mach) as env:
        env.info("info")
        env.debug("debug")

    assert env.called == 2


def test_multiple_mach_environement():
    mach = MagicMock()
    factories = [Env, Env, Env]

    with MultipleMachEnvironment(mach, factories) as env:
        env.info("info")
        env.debug("debug")

    for env in env.envs:
        assert env.called == 2


if __name__ == "__main__":
    mozunit.main()
