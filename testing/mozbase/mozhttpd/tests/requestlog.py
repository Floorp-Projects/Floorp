# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os

from six.moves.urllib.request import urlopen
import pytest

import mozhttpd
import mozunit


def log_requests(enabled):
    """Decorator to change the log_requests parameter for MozHttpd."""
    param_id = "enabled" if enabled else "disabled"
    return pytest.mark.parametrize("log_requests", [enabled], ids=[param_id])


@pytest.fixture(name="docroot")
def fixture_docroot():
    """Return a docroot path."""
    return os.path.dirname(os.path.abspath(__file__))


@pytest.fixture(name="request_log")
def fixture_request_log(docroot, log_requests):
    """Yields the request log of a started MozHttpd server."""
    httpd = mozhttpd.MozHttpd(
        port=0,
        docroot=docroot,
        log_requests=log_requests,
    )
    httpd.start(block=False)

    url = "http://{host}:{port}/".format(
        host="127.0.0.1",
        port=httpd.httpd.server_port,
    )
    f = urlopen(url)
    f.read()

    yield httpd.request_log

    httpd.stop()


@log_requests(True)
def test_logging_enabled(request_log):
    assert len(request_log) == 1
    log_entry = request_log[0]
    assert log_entry["method"] == "GET"
    assert log_entry["path"] == "/"
    assert type(log_entry["time"]) == float


@log_requests(False)
def test_logging_disabled(request_log):
    assert len(request_log) == 0


if __name__ == "__main__":
    mozunit.main()
