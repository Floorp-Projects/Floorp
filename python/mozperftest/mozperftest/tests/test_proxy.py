#!/usr/bin/env python
import mozunit
import os
import pytest
import shutil
import tempfile
import json
from unittest import mock

from mozperftest.tests.support import get_running_env
from mozperftest.environment import SYSTEM
from mozperftest.utils import silence
from mozperftest.system.proxy import OutputHandler


here = os.path.abspath(os.path.dirname(__file__))
example_dump = os.path.join(here, "..", "system", "example.zip")


class FakeOutputHandler:
    def finished(self):
        pass

    def wait_for_port(self):
        return 1234


class ProcHandler:
    def __init__(self, *args, **kw):
        self.args = args
        self.kw = kw
        self.pid = 1234

    def wait(self, *args):
        return

    run = wait

    @property
    def proc(self):
        return self


@pytest.fixture(scope="module")
def running_env():
    return get_running_env(proxy=True)


def mock_download_file(url, dest):
    shutil.copyfile(example_dump, dest)


@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("os.kill")
def test_replay(killer, running_env):
    mach_cmd, metadata, env = running_env
    system = env.layers[SYSTEM]
    env.set_arg("proxy-replay", example_dump)

    with system as proxy, silence():
        proxy(metadata)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_replay_url(killer, running_env):
    mach_cmd, metadata, env = running_env
    system = env.layers[SYSTEM]
    env.set_arg("proxy-replay", "http://example.dump")

    with system as proxy, silence():
        proxy(metadata)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_record(killer, running_env):
    mach_cmd, metadata, env = running_env
    system = env.layers[SYSTEM]
    with tempfile.TemporaryDirectory() as tmpdir:
        recording = os.path.join(tmpdir, "recording.dump")
        env.set_arg("proxy-record", recording)

        with system as proxy, silence():
            proxy(metadata)


@mock.patch("mozperftest.system.proxy.LOG")
def test_output_handler(logged):
    hdlr = OutputHandler()
    hdlr(b"simple line")
    hdlr(json.dumps({"not": "expected data"}).encode())

    # this catches the port
    hdlr(json.dumps({"action": "", "message": "Proxy running on port 1234"}).encode())
    assert hdlr.wait_for_port() == 1234


if __name__ == "__main__":
    mozunit.main()
