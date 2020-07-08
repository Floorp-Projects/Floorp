#!/usr/bin/env python
import mozunit
import os
import pytest

from mozbuild.base import MozbuildObject
from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM
from mozperftest.utils import install_package, silence

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture(scope="module")
def install_mozproxy():
    build = MozbuildObject.from_environment(cwd=here)
    build.virtualenv_manager.activate()

    mozbase = os.path.join(build.topsrcdir, "testing", "mozbase")
    mozproxy_deps = ["mozinfo", "mozlog", "mozproxy"]
    for i in mozproxy_deps:
        install_package(build.virtualenv_manager, os.path.join(mozbase, i))
    return build


def test_proxy(install_mozproxy):
    mach_cmd, metadata, env = get_running_env(proxy=True)
    system = env.layers[SYSTEM]

    # XXX this will run for real, we need to mock HTTP calls
    with system as proxy, silence():
        proxy(metadata)


if __name__ == "__main__":
    mozunit.main()
