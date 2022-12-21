#!/usr/bin/env python
import os
import platform
import subprocess
from pathlib import Path
from unittest import mock

import mozunit
import pytest

from mozperftest.system.macos import MacosDevice
from mozperftest.tests.support import DMG, get_running_env


def run_proc(*args, **kw):
    if args[0][1] == "attach":
        where = args[0][4]
        bindir = Path(where, "firefox.app", "Contents", "MacOS")
        os.makedirs(str(bindir))
        firefox_bin = bindir / "firefox"
        with firefox_bin.open("w") as f:
            f.write("OK")


def mock_calls(test):
    # on macOS we don't mock the system calls
    # so we're mounting for real using hdiutil
    if platform.system() == "Darwin":
        return test

    # on other platforms, we're unsing run_proc
    @mock.patch("mozperftest.system.macos.MacosDevice._run_process", new=run_proc)
    def wrapped(*args, **kw):
        return test(*args, **kw)


@mock_calls
def test_mount_dmg():
    mach_cmd, metadata, env = get_running_env(browsertime_binary=str(DMG))
    device = MacosDevice(env, mach_cmd)
    try:
        device.run(metadata)
    finally:
        device.teardown()

    target = Path(DMG.parent, "firefox", "Contents", "MacOS", "firefox")
    assert env.get_arg("browsertime-binary") == str(target)


def run_fail(cmd):
    def _run_fail(self, args):
        run_cmd = " ".join(args)
        if cmd not in run_cmd:
            run_proc(args)
            return
        raise subprocess.CalledProcessError(returncode=2, cmd=" ".join(args))

    return _run_fail


@mock.patch("mozperftest.system.macos.MacosDevice._run_process", new=run_fail("attach"))
def test_attach_fails():
    mach_cmd, metadata, env = get_running_env(browsertime_binary=str(DMG))
    device = MacosDevice(env, mach_cmd)

    with pytest.raises(subprocess.CalledProcessError):
        try:
            device.run(metadata)
        finally:
            device.teardown()


@mock.patch("mozperftest.system.macos.MacosDevice._run_process", new=run_fail("detach"))
def test_detach_fails():
    mach_cmd, metadata, env = get_running_env(browsertime_binary=str(DMG))
    device = MacosDevice(env, mach_cmd)
    # detaching will be swallowed
    try:
        device.run(metadata)
    finally:
        device.teardown()

    target = Path(DMG.parent, "firefox", "Contents", "MacOS", "firefox")
    assert env.get_arg("browsertime-binary") == str(target)


def test_no_op():
    mach_cmd, metadata, env = get_running_env(browsertime_binary="notadmg")
    device = MacosDevice(env, mach_cmd)
    device.run(metadata)


if __name__ == "__main__":
    mozunit.main()
