#!/usr/bin/env python
import mozunit
import os
import pytest
import shutil
import tempfile
import time
from unittest import mock

from mozbuild.base import MozbuildObject
from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM
from mozperftest.utils import install_package, silence

here = os.path.abspath(os.path.dirname(__file__))
example_dump = os.path.join(here, "..", "system", "example.dump")


@pytest.fixture(scope="module")
def install_mozproxy():
    build = MozbuildObject.from_environment(cwd=here)
    build.virtualenv_manager.activate()

    mozbase = os.path.join(build.topsrcdir, "testing", "mozbase")
    mozproxy_deps = ["mozinfo", "mozlog", "mozproxy"]
    for i in mozproxy_deps:
        install_package(build.virtualenv_manager, os.path.join(mozbase, i))
    return build


def mock_download_file(url, dest):
    shutil.copyfile(example_dump, dest)


def test_replay(install_mozproxy):
    mach_cmd, metadata, env = get_running_env(proxy=True)
    system = env.layers[SYSTEM]
    env.set_arg("proxy-replay", example_dump)

    # XXX this will run for real, we need to mock HTTP calls
    with system as proxy, silence():
        proxy(metadata)
        # Give mitmproxy a bit of time to start up so we can verify that it's
        # actually running before we tear things down.
        time.sleep(5)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
def test_replay_url(install_mozproxy):
    mach_cmd, metadata, env = get_running_env(proxy=True)
    system = env.layers[SYSTEM]
    env.set_arg("proxy-replay", "http://example.dump")

    # XXX this will run for real, we need to mock HTTP calls
    with system as proxy, silence():
        proxy(metadata)
        # Give mitmproxy a bit of time to start up so we can verify that it's
        # actually running before we tear things down.
        time.sleep(5)


def test_record(install_mozproxy):
    mach_cmd, metadata, env = get_running_env(proxy=True)
    system = env.layers[SYSTEM]
    with tempfile.TemporaryDirectory() as tmpdir:
        recording = os.path.join(tmpdir, "recording.dump")
        env.set_arg("proxy-record", recording)

        # XXX this will run for real, we need to mock HTTP calls
        with system as proxy, silence():
            proxy(metadata)
        assert os.path.exists(recording)


if __name__ == "__main__":
    mozunit.main()
