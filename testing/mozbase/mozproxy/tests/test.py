#!/usr/bin/env python
from __future__ import absolute_import, print_function
import os
import subprocess

import mozprocess
import mozunit
import mozinfo
from mozproxy import get_playback
from mozproxy import utils


here = os.path.dirname(__file__)
# XXX we need to create a temp dir where things happen, for cleanup
# otherwise, the source tree gets polluted

# XXX will do better later using a real mock tool


def _no_download(*args, **kw):
    pass


utils.tooltool_download = _no_download


def check_output(*args, **kw):
    return "yea ok"


subprocess.check_output = check_output


class ProcessHandler:
    def __init__(self, *args, **kw):
        self.pid = 12345

    def run(self):
        pass

    def poll(self):
        return None


mozprocess.ProcessHandler = ProcessHandler


def test_mitm():
    from mozproxy.backends import mitm

    mitm.MITMDUMP_SLEEP = 0.1

    bin_name = "mitmproxy-rel-bin-{platform}.manifest"
    pageset_name = "mitmproxy-recordings-raptor-paypal.manifest"

    config = {
        "playback_tool": "mitmproxy",
        "playback_binary_manifest": bin_name,
        "playback_pageset_manifest": pageset_name,
        "platform": mozinfo.os,
        "playback_recordings": os.path.join(here, "paypal.mp"),
        "run_local": True,
        "obj_path": here,   # XXX tmp?
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    playback = get_playback(config)
    assert playback is not None


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
