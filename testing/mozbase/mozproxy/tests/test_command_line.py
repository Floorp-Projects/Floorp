#!/usr/bin/env python
from __future__ import absolute_import
import json
import os
import re
import signal
import threading

import mozunit
import pytest
from mozbuild.base import MozbuildObject
from mozprocess import ProcessHandler

here = os.path.abspath(os.path.dirname(__file__))


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
    virtualenv_manager._run_pip(["install", package])


def _kill_mozproxy(pid):
    kill_signal = getattr(signal, "CTRL_BREAK_EVENT", signal.SIGINT)
    os.kill(pid, kill_signal)


class OutputHandler(object):
    def __init__(self):
        self.port = None
        self.port_event = threading.Event()

    def __call__(self, line):
        if not line.strip():
            return
        line = line.decode("utf-8", errors="replace")

        try:
            data = json.loads(line)
        except ValueError:
            return

        if isinstance(data, dict) and "action" in data:
            # Retrieve the port number for the proxy server from the logs of
            # our subprocess.
            m = re.match(r"Proxy running on port (\d+)",
                         data.get("message", ""))
            if m:
                self.port = m.group(1)
                self.port_event.set()

    def finished(self):
        self.port_event.set()


@pytest.fixture(scope="module")
def install_mozproxy():
    build = MozbuildObject.from_environment(cwd=here)
    build.virtualenv_manager.activate()

    mozbase = os.path.join(build.topsrcdir, "testing", "mozbase")
    mozproxy_deps = ["mozinfo", "mozlog", "mozproxy"]
    for i in mozproxy_deps:
        _install_package(build.virtualenv_manager, os.path.join(mozbase, i))
    return build


def test_help(install_mozproxy):
    p = ProcessHandler(["mozproxy", "--help"])
    p.run()
    assert p.wait() == 0


def test_run(install_mozproxy):
    build = install_mozproxy
    output_handler = OutputHandler()
    p = ProcessHandler(
        ["mozproxy",
         "--local",
         "--binary=firefox",
         "--topsrcdir=" + build.topsrcdir,
         "--objdir=" + build.topobjdir],
        processOutputLine=output_handler,
        onFinish=output_handler.finished,
    )
    p.run()
    # The first time we run mozproxy, we need to fetch mitmproxy, which can
    # take a while...
    assert output_handler.port_event.wait(120) is True
    _kill_mozproxy(p.pid)

    assert p.wait(10) == 0
    assert output_handler.port is not None


def test_failure(install_mozproxy):
    output_handler = OutputHandler()
    p = ProcessHandler(
        ["mozproxy",
         "--local",
         # Exclude some options here to trigger a command-line error.
         os.path.join(here, "example.dump")],
        processOutputLine=output_handler,
        onFinish=output_handler.finished,
    )
    p.run()
    assert output_handler.port_event.wait(10) is True
    assert p.wait(10) == 2
    assert output_handler.port is None


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
