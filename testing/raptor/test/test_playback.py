import os
import time

import mozinfo
import mozunit
from mozlog.structuredlog import StructuredLogger, set_default_logger

# need this so raptor imports work both from /raptor and via mach
here = os.path.abspath(os.path.dirname(__file__))

set_default_logger(StructuredLogger("test_playback"))


from mozproxy import get_playback
from mozproxy.backends.mitm.desktop import MitmproxyDesktop

config = {}

run_local = True
if os.environ.get("TOOLTOOLCACHE") is None:
    run_local = False


def test_get_playback(get_binary):
    config["platform"] = mozinfo.os
    if "win" in config["platform"]:
        # this test is not yet supported on windows
        assert True
        return
    config["obj_path"] = os.path.dirname(get_binary("firefox"))
    config["playback_tool"] = "mitmproxy"
    config["playback_version"] = "8.1.1"
    config["playback_files"] = [
        os.path.join(
            os.path.dirname(os.path.abspath(os.path.dirname(__file__))),
            "raptor",
            "tooltool-manifests",
            "playback",
            "mitm8-linux-firefox-amazon.manifest",
        )
    ]
    config["binary"] = get_binary("firefox")
    config["run_local"] = run_local
    config["app"] = "firefox"
    config["host"] = "127.0.0.1"

    playback = get_playback(config)
    assert isinstance(playback, MitmproxyDesktop)
    playback.start()
    time.sleep(1)
    playback.stop()


def test_get_unsupported_playback():
    config["playback_tool"] = "unsupported"
    playback = get_playback(config)
    assert playback is None


def test_get_playback_missing_tool_name():
    playback = get_playback(config)
    assert playback is None


if __name__ == "__main__":
    mozunit.main()
