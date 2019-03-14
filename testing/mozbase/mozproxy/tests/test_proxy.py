#!/usr/bin/env python
from __future__ import absolute_import, print_function
import os

import mock
import mozunit
import mozinfo
from mozproxy import get_playback
from mozproxy.backends import mitm

here = os.path.dirname(__file__)


class Process:
    def __init__(self, *args, **kw):
        pass

    def poll(self):
        return None

    wait = poll

    def kill(self, sig=9):
        pass

    pid = 1234
    stderr = stdout = None


@mock.patch("mozprocess.processhandler.ProcessHandlerMixin.Process", new=Process)
@mock.patch("mozproxy.backends.mitm.tooltool_download", new=mock.DEFAULT)
def test_mitm(*args):
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
        "obj_path": here,  # XXX tmp?
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    playback = get_playback(config)
    assert playback is not None
    try:
        playback.start()
    finally:
        playback.stop()


@mock.patch("mozprocess.processhandler.ProcessHandlerMixin.Process", new=Process)
@mock.patch("mozproxy.backends.mitm.tooltool_download", new=mock.DEFAULT)
def test_playback_setup_failed(*args):
    class SetupFailed(Exception):
        pass

    def setup(*args, **kw):
        def _s(self):
            raise SetupFailed("Failed")

        return _s

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
        "obj_path": here,  # XXX tmp?
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    prefix = "mozproxy.backends.mitm.MitmproxyDesktop."
    with mock.patch(prefix + "setup", new_callable=setup):
        with mock.patch(prefix + "stop_mitmproxy_playback") as p:
            try:
                pb = get_playback(config)
                pb.start()
            except SetupFailed:
                assert p.call_count == 1
            except Exception:
                raise


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
