from __future__ import absolute_import, unicode_literals

import mozunit
import os
import requests
import sys
import mock

from BaseHTTPServer import HTTPServer
from mozlog.structuredlog import set_default_logger, StructuredLogger
from raptor.control_server import RaptorControlServer

# need this so the raptor unit tests can find output & filter classes
here = os.path.abspath(os.path.dirname(__file__))
raptor_dir = os.path.join(os.path.dirname(here), 'raptor')
sys.path.insert(0, raptor_dir)

from raptor.results import RaptorResultsHandler


set_default_logger(StructuredLogger('test_control_server'))


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


def test_server_get_timeout(raptor):
    test_name = "test-name"
    url = "test-url"
    metrics = {"metric1": False, "metric2": True, "metric3": True}

    def post_state():
        requests.post(
            "http://127.0.0.1:%s/" % raptor.control_server.port,
            json={
                "type": "webext_raptor-page-timeout",
                "data": [test_name, url, metrics]
            })

    assert len(raptor.results_handler.page_timeout_list) == 0

    post_state()

    assert len(raptor.results_handler.page_timeout_list) == 1

    timeout_details = raptor.results_handler.page_timeout_list[0]
    assert timeout_details['test_name'] == test_name
    assert timeout_details['url'] == url

    pending_metrics = [k for k, v in metrics.items() if v]
    assert len(timeout_details['pending_metrics'].split(', ')) == len(pending_metrics)


def test_server_android_app_backgrounding():
    # Mock the background and foreground functions
    with mock.patch.object(
            RaptorControlServer, 'background_app', return_value=True
         ) as _, \
         mock.patch.object(
            RaptorControlServer, 'foreground_app', return_value=True
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
                    "data": "starting background app"
                })

        def post_end_background():
            requests.post(
                "http://127.0.0.1:%s/" % control.port,
                json={
                    "type": "webext_end_background",
                    "data": "ending background app"
                })

        # Test that app is backgrounded
        post_start_background()
        assert control.background_app.assert_called()

        # Test that app is returned to foreground
        post_end_background()
        assert control.foreground_app.assert_called()

        # Make sure the control server stops after these requests
        control.stop()
        assert not control._server_thread.is_alive()


if __name__ == '__main__':
    mozunit.main()
