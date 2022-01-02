from __future__ import absolute_import

import pytest

import mozhttpd
import mozunit


@pytest.fixture(name="httpd")
def fixture_httpd():
    """Yields a started MozHttpd server."""
    httpd = mozhttpd.MozHttpd(port=0)
    httpd.start(block=False)
    yield httpd
    httpd.stop()


def test_base_url(httpd):
    port = httpd.httpd.server_port

    want = "http://127.0.0.1:{}/".format(port)
    got = httpd.get_url()
    assert got == want

    want = "http://127.0.0.1:{}/cheezburgers.html".format(port)
    got = httpd.get_url(path="/cheezburgers.html")
    assert got == want


def test_base_url_when_not_started():
    httpd = mozhttpd.MozHttpd(port=0)
    assert httpd.get_url() is None


if __name__ == "__main__":
    mozunit.main()
