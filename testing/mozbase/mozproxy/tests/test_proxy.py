#!/usr/bin/env python
from __future__ import absolute_import, print_function
import os

import mock
import mozunit
import mozinfo
import requests
from mozproxy import get_playback
from support import tempdir

here = os.path.dirname(__file__)


class Process:
    def __init__(self, *args, **kw):
        pass

    def run(self):
        print("I am running something")

    def poll(self):
        return None

    def wait(self):
        return 0

    def kill(self, sig=9):
        pass

    proc = object()
    pid = 1234
    stderr = stdout = None
    returncode = 0


_RETRY = 0


class ProcessWithRetry(Process):
    def __init__(self, *args, **kw):
        Process.__init__(self, *args, **kw)

    def wait(self):
        global _RETRY
        _RETRY += 1
        if _RETRY >= 2:
            _RETRY = 0
            return 0
        return -1


def kill(pid, signal):
    if pid == 1234:
        return
    return os.kill(pid, signal)


def get_status_code(url, playback):
    response = requests.get(url=url,
                            proxies={"http": "http://%s:%s/" % (playback.host, playback.port)})
    return response.status_code


def test_mitm_check_proxy(*args):
    # test setup
    bin_name = "mitmproxy-rel-bin-4.0.4-{platform}.manifest"
    pageset_name = "mitm4-linux-firefox-amazon.manifest"
    playback_recordings = "amazon.mp"

    config = {
        "playback_tool": "mitmproxy",
        "playback_binary_manifest": bin_name,
        "playback_pageset_manifest": os.path.join(here, "files", pageset_name),
        "playback_version": '4.0.4',
        "platform": mozinfo.os,
        "run_local": "MOZ_AUTOMATION" not in os.environ,
        "binary": "firefox",
        "app": "firefox",
        "host": "127.0.0.1",
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        playback = get_playback(config)
        playback.config['playback_files'] = [
            os.path.join(obj_path, "testing", "mozproxy", playback_recordings)]
        assert playback is not None

        try:
            playback.start()

            url = "https://m.media-amazon.com/images/G/01/csm/showads.v2.js"
            assert get_status_code(url, playback) == 200

            url = "http://mozproxy/checkProxy"
            assert get_status_code(url, playback) == 404
        finally:
            playback.stop()


@mock.patch("mozproxy.backends.mitm.Mitmproxy.check_proxy")
@mock.patch("mozproxy.backends.mitm.mitm.ProcessHandler", new=Process)
@mock.patch("mozproxy.utils.ProcessHandler", new=Process)
@mock.patch("os.kill", new=kill)
def test_mitm(*args):
    bin_name = "mitmproxy-rel-bin-4.0.4-{platform}.manifest"
    pageset_name = "mitm4-linux-firefox-amazon.manifest"

    config = {
        "playback_tool": "mitmproxy",
        "playback_binary_manifest": bin_name,
        "playback_pageset_manifest": pageset_name,
        "playback_version": '4.0.4',
        "platform": mozinfo.os,
        "playback_recordings": os.path.join(here, "paypal.mp"),
        "run_local": True,
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        playback = get_playback(config)
        playback.config['playback_files'] = config['playback_recordings']
    assert playback is not None
    try:
        playback.start()
    finally:
        playback.stop()


@mock.patch("mozproxy.backends.mitm.Mitmproxy.check_proxy")
@mock.patch("mozproxy.backends.mitm.mitm.ProcessHandler", new=Process)
@mock.patch("mozproxy.utils.ProcessHandler", new=Process)
@mock.patch("os.kill", new=kill)
def test_playback_setup_failed(*args):
    class SetupFailed(Exception):
        pass

    def setup(*args, **kw):
        def _s(self):
            raise SetupFailed("Failed")

        return _s

    bin_name = "mitmproxy-rel-bin-4.0.4-{platform}.manifest"
    pageset_name = "mitm4-linux-firefox-amazon.manifest"

    config = {
        "playback_tool": "mitmproxy",
        "playback_binary_manifest": bin_name,
        "playback_pageset_manifest": pageset_name,
        "playback_version": '4.0.4',
        "platform": mozinfo.os,
        "playback_recordings": os.path.join(here, "paypal.mp"),
        "run_local": True,
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    prefix = "mozproxy.backends.mitm.MitmproxyDesktop."

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        with mock.patch(prefix + "setup", new_callable=setup):
            with mock.patch(prefix + "stop_mitmproxy_playback") as p:
                try:
                    pb = get_playback(config)
                    pb.config['playback_files'] = config['playback_recordings']
                    pb.start()
                except SetupFailed:
                    assert p.call_count == 1
                except Exception:
                    raise


@mock.patch("mozproxy.backends.mitm.Mitmproxy.check_proxy")
@mock.patch("mozproxy.backends.mitm.mitm.ProcessHandler", new=ProcessWithRetry)
@mock.patch("mozproxy.utils.ProcessHandler", new=ProcessWithRetry)
@mock.patch("os.kill", new=kill)
def test_mitm_with_retry(*args):
    bin_name = "mitmproxy-rel-bin-4.0.4-{platform}.manifest"
    pageset_name = "mitm4-linux-firefox-amazon.manifest"

    config = {
        "playback_tool": "mitmproxy",
        "playback_binary_manifest": bin_name,
        "playback_pageset_manifest": pageset_name,
        "playback_version": '4.0.4',
        "platform": mozinfo.os,
        "playback_recordings": os.path.join(here, "paypal.mp"),
        "run_local": True,
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        playback = get_playback(config)
        playback.config['playback_files'] = config['playback_recordings']
    assert playback is not None
    try:
        playback.start()
    finally:
        playback.stop()


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
