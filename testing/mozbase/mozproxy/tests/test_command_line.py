#!/usr/bin/env python
import json
import os
import re
import signal
import subprocess
import sys
import threading
import time

import mozunit
from buildconfig import topobjdir, topsrcdir

here = os.path.dirname(__file__)

if os.name == "nt":
    PROCESS_CREATION_FLAGS = subprocess.CREATE_NEW_PROCESS_GROUP
else:
    PROCESS_CREATION_FLAGS = 0


# This is copied from <python/mozperftest/mozperftest/utils.py>. It's copied
# instead of imported since mozperfest is Python 3, and this file is
# (currently) Python 2.
def _install_package(virtualenv_manager, package):
    from pip._internal.req.constructors import install_req_from_line

    req = install_req_from_line(package)
    req.check_if_exists(use_user_site=False)
    # already installed, check if it's in our venv
    if req.satisfied_by is not None:
        venv_site_lib = os.path.abspath(
            os.path.join(virtualenv_manager.bin_path, "..", "lib")
        )
        site_packages = os.path.abspath(req.satisfied_by.location)
        if site_packages.startswith(venv_site_lib):
            # already installed in this venv, we can skip
            return

    subprocess.check_call(
        [
            virtualenv_manager.python_path,
            "-m",
            "pip",
            "install",
            package,
        ]
    )


def _kill_mozproxy(pid):
    kill_signal = getattr(signal, "CTRL_BREAK_EVENT", signal.SIGINT)
    os.kill(pid, kill_signal)


class OutputHandler(object):
    def __init__(self):
        self.port = None
        self.port_event = threading.Event()

    def __call__(self, line):
        line = line.rstrip(b"\r\n")
        if not line.strip():
            return
        line = line.decode("utf-8", errors="replace")
        # Print the output we received so we have useful logs if a test fails.
        print(line)

        try:
            data = json.loads(line)
        except ValueError:
            return

        if isinstance(data, dict) and "action" in data:
            # Retrieve the port number for the proxy server from the logs of
            # our subprocess.
            m = re.match(r"Proxy running on port (\d+)", data.get("message", ""))
            if m:
                self.port = m.group(1)
                self.port_event.set()

    def finished(self):
        self.port_event.set()


def test_help():
    p = subprocess.run([sys.executable, "-m", "mozproxy", "--help"])
    assert p.returncode == 0


def test_run_record_no_files():
    output_handler = OutputHandler()
    p = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "mozproxy",
            "--local",
            "--mode=record",
            "--binary=firefox",
            "--topsrcdir=" + topsrcdir,
            "--objdir=" + topobjdir,
        ],
        creationflags=PROCESS_CREATION_FLAGS,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )

    for line in p.stdout:
        output_handler(line)
        if output_handler.port_event.is_set():
            break
    output_handler.finished()
    # The first time we run mozproxy, we need to fetch mitmproxy, which can
    # take a while...
    assert output_handler.port_event.wait(120) is True
    # Give mitmproxy a bit of time to start up so we can verify that it's
    # actually running before we kill mozproxy.
    time.sleep(5)
    _kill_mozproxy(p.pid)

    # Assert process raises error
    assert p.wait(10) == 2
    assert output_handler.port is None


def test_run_record_multiple_files():
    output_handler = OutputHandler()
    p = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "mozproxy",
            "--local",
            "--mode=record",
            "--binary=firefox",
            "--topsrcdir=" + topsrcdir,
            "--objdir=" + topobjdir,
            os.path.join(here, "files", "new_record.zip"),
            os.path.join(here, "files", "new_record2.zip"),
        ],
        creationflags=PROCESS_CREATION_FLAGS,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )

    for line in p.stdout:
        output_handler(line)
        if output_handler.port_event.is_set():
            break
    output_handler.finished()
    # The first time we run mozproxy, we need to fetch mitmproxy, which can
    # take a while...
    assert output_handler.port_event.wait(120) is True
    # Give mitmproxy a bit of time to start up so we can verify that it's
    # actually running before we kill mozproxy.
    time.sleep(5)
    _kill_mozproxy(p.pid)

    assert p.wait(10) == 4
    assert output_handler.port is None


def test_run_record():
    output_handler = OutputHandler()
    p = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "mozproxy",
            "--local",
            "--mode=record",
            "--binary=firefox",
            "--topsrcdir=" + topsrcdir,
            "--objdir=" + topobjdir,
            os.path.join(here, "files", "record.zip"),
        ],
        creationflags=PROCESS_CREATION_FLAGS,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )

    for line in p.stdout:
        output_handler(line)
        if output_handler.port_event.is_set():
            break
    output_handler.finished()
    try:
        # The first time we run mozproxy, we need to fetch mitmproxy, which can
        # take a while...
        assert output_handler.port_event.wait(120) is True
        # Give mitmproxy a bit of time to start up so we can verify that it's
        # actually running before we kill mozproxy.
        time.sleep(5)
        _kill_mozproxy(p.pid)

        assert p.wait(10) == 0
        assert output_handler.port is not None
    finally:
        os.remove(os.path.join(here, "files", "record.zip"))


def test_run_playback():
    output_handler = OutputHandler()
    p = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "mozproxy",
            "--local",
            "--binary=firefox",
            "--topsrcdir=" + topsrcdir,
            "--objdir=" + topobjdir,
            os.path.join(here, "files", "mitm5-linux-firefox-amazon.zip"),
        ],
        creationflags=PROCESS_CREATION_FLAGS,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )

    for line in p.stdout:
        output_handler(line)
        if output_handler.port_event.is_set():
            break
    output_handler.finished()
    # The first time we run mozproxy, we need to fetch mitmproxy, which can
    # take a while...
    assert output_handler.port_event.wait(120) is True
    # Give mitmproxy a bit of time to start up so we can verify that it's
    # actually running before we kill mozproxy.
    time.sleep(5)
    _kill_mozproxy(p.pid)

    assert p.wait(10) == 0
    assert output_handler.port is not None


def test_failure():
    output_handler = OutputHandler()
    p = subprocess.Popen(
        [
            sys.executable,
            "-m",
            "mozproxy",
            "--local",
            # Exclude some options here to trigger a command-line error.
            os.path.join(here, "files", "mitm5-linux-firefox-amazon.zip"),
        ],
        creationflags=PROCESS_CREATION_FLAGS,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=False,
    )
    for line in p.stdout:
        output_handler(line)
        if output_handler.port_event.is_set():
            break
    output_handler.finished()
    assert output_handler.port_event.wait(10) is True
    assert p.wait(10) == 2
    assert output_handler.port is None


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
