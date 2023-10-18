#!/usr/bin/env python
import os
from unittest import mock

import mozinfo
import mozunit
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

    def communicate(self):
        return (["mock stdout"], ["mock stderr"])

    def wait(self):
        return 0

    def kill(self, sig=9):
        pass

    proc = object()
    pid = 1234
    stderr = stdout = []
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
    response = requests.get(
        url=url, proxies={"http": "http://%s:%s/" % (playback.host, playback.port)}
    )
    return response.status_code


def cleanup():
    # some tests create this file as a side-effect
    policies_file = os.path.join("distribution", "policies.json")
    try:
        if os.path.exists(policies_file):
            os.remove(policies_file)
    except PermissionError:
        pass


def test_mitm_check_proxy(*args):
    # test setup
    pageset_name = os.path.join(here, "files", "mitm5-linux-firefox-amazon.manifest")

    config = {
        "playback_tool": "mitmproxy",
        "playback_files": [os.path.join(here, "files", pageset_name)],
        "playback_version": "8.1.1",
        "platform": mozinfo.os,
        "run_local": "MOZ_AUTOMATION" not in os.environ,
        "binary": "firefox",
        "app": "firefox",
        "host": "127.0.0.1",
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        playback = get_playback(config)
        assert playback is not None

        try:
            playback.start()

            url = "https://m.media-amazon.com/images/G/01/csm/showads.v2.js"
            assert get_status_code(url, playback) == 200

            url = "http://mozproxy/checkProxy"
            assert get_status_code(url, playback) == 404
        finally:
            playback.stop()
    cleanup()


@mock.patch("mozproxy.backends.mitm.Mitmproxy.check_proxy")
@mock.patch("mozproxy.backends.mitm.mitm.ProcessHandler", new=Process)
@mock.patch("mozproxy.utils.Popen", new=Process)
@mock.patch("os.kill", new=kill)
def test_mitm(*args):
    pageset_name = os.path.join(here, "files", "mitm5-linux-firefox-amazon.manifest")

    config = {
        "playback_tool": "mitmproxy",
        "playback_files": [pageset_name],
        "playback_version": "8.1.1",
        "platform": mozinfo.os,
        "run_local": True,
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        playback = get_playback(config)
    assert playback is not None
    try:
        playback.start()
    finally:
        playback.stop()
    cleanup()


@mock.patch("mozproxy.backends.mitm.Mitmproxy.check_proxy")
@mock.patch("mozproxy.backends.mitm.mitm.ProcessHandler", new=Process)
@mock.patch("mozproxy.utils.Popen", new=Process)
@mock.patch("os.kill", new=kill)
def test_playback_setup_failed(*args):
    class SetupFailed(Exception):
        pass

    def setup(*args, **kw):
        def _s(self):
            raise SetupFailed("Failed")

        return _s

    pageset_name = os.path.join(here, "files", "mitm5-linux-firefox-amazon.manifest")

    config = {
        "playback_tool": "mitmproxy",
        "playback_files": [pageset_name],
        "playback_version": "4.0.4",
        "platform": mozinfo.os,
        "run_local": True,
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    prefix = "mozproxy.backends.mitm.desktop.MitmproxyDesktop."

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        with mock.patch(prefix + "setup", new_callable=setup):
            with mock.patch(prefix + "stop_mitmproxy_playback") as p:
                try:
                    pb = get_playback(config)
                    pb.start()
                except SetupFailed:
                    assert p.call_count == 1
                except Exception:
                    raise


@mock.patch("mozproxy.backends.mitm.Mitmproxy.check_proxy")
@mock.patch("mozproxy.backends.mitm.mitm.ProcessHandler", new=ProcessWithRetry)
@mock.patch("mozproxy.utils.Popen", new=ProcessWithRetry)
@mock.patch("os.kill", new=kill)
def test_mitm_with_retry(*args):
    pageset_name = os.path.join(here, "files", "mitm5-linux-firefox-amazon.manifest")

    config = {
        "playback_tool": "mitmproxy",
        "playback_files": [pageset_name],
        "playback_version": "8.1.1",
        "platform": mozinfo.os,
        "run_local": True,
        "binary": "firefox",
        "app": "firefox",
        "host": "example.com",
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        playback = get_playback(config)
    assert playback is not None
    try:
        playback.start()
    finally:
        playback.stop()
    cleanup()


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
