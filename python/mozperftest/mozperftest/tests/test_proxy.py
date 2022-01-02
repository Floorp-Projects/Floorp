#!/usr/bin/env python
import json
import os
import shutil
import tempfile
from unittest import mock

import mozunit
import pytest
from mozperftest.environment import SYSTEM
from mozperftest.system.proxy import OutputHandler
from mozperftest.tests.support import get_running_env
from mozperftest.utils import silence

here = os.path.abspath(os.path.dirname(__file__))
example_dump = os.path.join(here, "..", "system", "example.zip")


class FakeOutputHandler:
    def finished(self):
        pass

    def wait_for_port(self):
        return 1234


class FakeOutputHandlerFail:
    def finished(self):
        pass

    def wait_for_port(self):
        return None


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


class ProcHandlerError:
    def __init__(self, *args, **kw):
        self.args = args
        self.kw = kw
        self.pid = 1234

    def wait(self, *args):
        return 1

    run = wait

    @property
    def proc(self):
        return self


class FakeDevice:
    def create_socket_connection(self, direction, local, remote):
        return "A Fake socket"


def running_env():
    return get_running_env(proxy=True)


def mock_download_file(url, dest):
    shutil.copyfile(example_dump, dest)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandlerFail)
@mock.patch("os.kill")
def test_port_error(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    with tempfile.TemporaryDirectory() as tmpdir:
        recording = os.path.join(tmpdir, "recording.zip")
        env.set_arg("proxy-mode", "record")
        env.set_arg("proxy-file", recording)

        with system as proxy, pytest.raises(ValueError) as excinfo, silence():
            proxy(metadata)
        assert "Unable to retrieve the port number from mozproxy" in str(excinfo.value)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandlerError)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_proxy_error(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    with tempfile.TemporaryDirectory() as tmpdir:
        recording = os.path.join(tmpdir, "recording.zip")
        env.set_arg("proxy-mode", "record")
        env.set_arg("proxy-file", recording)

        with pytest.raises(ValueError) as excinfo:
            with system as proxy, silence():
                proxy(metadata)
        assert "mozproxy terminated early with return code 1" in str(excinfo.value)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_playback_no_file(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    env.set_arg("proxy-mode", "playback")

    with system as proxy, pytest.raises(ValueError) as excinfo, silence():
        proxy(metadata)
    assert "Proxy file not provided!!" in str(excinfo.value)


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_playback_no_mode(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    env.set_arg("proxy-file", example_dump)

    with system as proxy, pytest.raises(ValueError) as excinfo, silence():
        proxy(metadata)
    assert "Proxy mode not provided please provide proxy mode" in str(excinfo.value)


@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.ADBDevice", new=FakeDevice)
@mock.patch("os.kill")
def test_android_proxy(killer):
    mach_cmd, metadata, env = running_env()
    metadata.flavor = "mobile-browser"
    system = env.layers[SYSTEM]
    env.set_arg("proxy-mode", "playback")
    env.set_arg("proxy-file", example_dump)

    with system as proxy, silence():
        proxy(metadata)

    browser_prefs = metadata.get_options("browser_prefs")
    assert browser_prefs["network.proxy.http_port"] == 1234


@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("os.kill")
def test_replay(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    env.set_arg("proxy-mode", "playback")
    env.set_arg("proxy-file", example_dump)

    with system as proxy, silence():
        proxy(metadata)

    browser_prefs = metadata.get_options("browser_prefs")
    assert browser_prefs["network.proxy.http_port"] == 1234


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_replay_url(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    env.set_arg("proxy-mode", "playback")
    env.set_arg("proxy-file", "http://example.dump")

    with system as proxy, silence():
        proxy(metadata)

    browser_prefs = metadata.get_options("browser_prefs")
    assert browser_prefs["network.proxy.http_port"] == 1234


@mock.patch("mozperftest.system.proxy.download_file", mock_download_file)
@mock.patch("mozperftest.system.proxy.ProcessHandler", new=ProcHandler)
@mock.patch("mozperftest.system.proxy.OutputHandler", new=FakeOutputHandler)
@mock.patch("os.kill")
def test_record(killer):
    mach_cmd, metadata, env = running_env()
    system = env.layers[SYSTEM]
    with tempfile.TemporaryDirectory() as tmpdir:
        recording = os.path.join(tmpdir, "recording.zip")
        env.set_arg("proxy-mode", "record")
        env.set_arg("proxy-file", recording)

        with system as proxy, silence():
            proxy(metadata)

        browser_prefs = metadata.get_options("browser_prefs")
        assert browser_prefs["network.proxy.http_port"] == 1234


@mock.patch("mozperftest.system.proxy.LOG")
def test_output_handler(logged):
    hdlr = OutputHandler()

    hdlr(b"")
    hdlr(b"simple line")
    hdlr(json.dumps({"not": "expected data"}).encode())

    hdlr.finished()
    assert hdlr.wait_for_port() is None

    # this catches the port
    hdlr(json.dumps({"action": "", "message": "Proxy running on port 1234"}).encode())
    assert hdlr.wait_for_port() == 1234


if __name__ == "__main__":
    mozunit.main()
