import os
import shutil
import sys
from unittest import mock

import mozunit
import requests

try:
    from http.server import HTTPServer  # py3
except ImportError:
    from BaseHTTPServer import HTTPServer  # py2

from mozlog.structuredlog import StructuredLogger, set_default_logger
from raptor.control_server import RaptorControlServer

# need this so the raptor unit tests can find output & filter classes
here = os.path.abspath(os.path.dirname(__file__))
raptor_dir = os.path.join(os.path.dirname(here), "raptor")
sys.path.insert(0, raptor_dir)

from raptor.results import RaptorResultsHandler

set_default_logger(StructuredLogger("test_control_server"))


def clear_cache():
    # remove the condprof download cache
    from condprof.client import CONDPROF_CACHE  # noqa

    if os.path.exists(CONDPROF_CACHE):
        shutil.rmtree(CONDPROF_CACHE)


clear_cache()


def test_start_and_stop():

    results_handler = RaptorResultsHandler()
    control = RaptorControlServer(results_handler)

    assert control.server is None
    control.start()
    assert isinstance(control.server, HTTPServer)
    assert control.server.fileno()
    assert control._server_thread.is_alive()

    control.stop()
    assert not control._server_thread.is_alive()


def test_server_android_app_backgrounding():
    # Mock the background and foreground functions
    with mock.patch.object(
        RaptorControlServer, "background_app", return_value=True
    ) as _, mock.patch.object(
        RaptorControlServer, "foreground_app", return_value=True
    ) as _:

        results_handler = RaptorResultsHandler()
        control = RaptorControlServer(results_handler)
        control.backgrounded = False

        control.start()
        assert control._server_thread.is_alive()

        def post_start_background():
            requests.post(
                "http://127.0.0.1:%s/" % control.port,
                json={
                    "type": "webext_start_background",
                    "data": "starting background app",
                },
            )

        def post_end_background():
            requests.post(
                "http://127.0.0.1:%s/" % control.port,
                json={"type": "webext_end_background", "data": "ending background app"},
            )

        # Test that app is backgrounded
        post_start_background()
        control.background_app.assert_called()

        # Test that app is returned to foreground
        post_end_background()
        control.foreground_app.assert_called()

        # Make sure the control server stops after these requests
        control.stop()
        assert not control._server_thread.is_alive()


if __name__ == "__main__":
    mozunit.main()
