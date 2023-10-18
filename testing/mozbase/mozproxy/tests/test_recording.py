#!/usr/bin/env python
import datetime
import os
from builtins import Exception

import mozinfo
import mozunit
import requests
from mozproxy import get_playback
from support import tempdir

here = os.path.dirname(__file__)
os.environ["MOZPROXY_DIR"] = os.path.join(here, "files")


def get_status_code(url, playback):
    response = requests.get(
        url=url, proxies={"http": "http://%s:%s/" % (playback.host, playback.port)}
    )
    return response.status_code


def test_record_and_replay(*args):
    # test setup

    basename = "recording"
    suffix = datetime.datetime.now().strftime("%y%m%d_%H%M%S")
    filename = "_".join([basename, suffix])
    recording_file = os.path.join(here, "files", ".".join([filename, "zip"]))

    # Record part
    config = {
        "playback_tool": "mitmproxy",
        "recording_file": recording_file,
        "playback_version": "8.1.1",
        "platform": mozinfo.os,
        "run_local": "MOZ_AUTOMATION" not in os.environ,
        "binary": "firefox",
        "app": "firefox",
        "host": "127.0.0.1",
        "record": True,
    }

    with tempdir() as obj_path:
        config["obj_path"] = obj_path
        record = get_playback(config)
        assert record is not None

        try:
            record.start()

            url = "https://m.media-amazon.com/images/G/01/csm/showads.v2.js"
            assert get_status_code(url, record) == 200
        finally:
            record.stop()

        # playback part
        config["record"] = False
        config["recording_file"] = None
        config["playback_files"] = [recording_file]
        playback = get_playback(config)
        assert playback is not None
        try:
            playback.start()

            url = "https://m.media-amazon.com/images/G/01/csm/showads.v2.js"
            assert get_status_code(url, playback) == 200
        finally:
            playback.stop()

    # Cleanup
    try:
        os.remove(recording_file)
        os.remove(os.path.join("distribution", "policies.json"))
    except Exception:
        pass


if __name__ == "__main__":
    mozunit.main(runwith="pytest")
