#!/usr/bin/env python

"""
tests for mozfile.load
"""

from __future__ import absolute_import

import mozunit
import pytest

from wptserve.handlers import handler
from wptserve.server import WebTestHttpd

from mozfile import load


@pytest.fixture(name="httpd_url")
def fixture_httpd_url():
    """Yield a started WebTestHttpd server."""

    @handler
    def example(request, response):
        """Example request handler."""
        body = b"example"
        return (
            200,
            [("Content-type", "text/plain"), ("Content-length", len(body))],
            body,
        )

    httpd = WebTestHttpd(host="127.0.0.1", routes=[("GET", "*", example)])

    httpd.start(block=False)
    yield httpd.get_url()
    httpd.stop()


def test_http(httpd_url):
    """Test with WebTestHttpd and a http:// URL."""
    content = load(httpd_url).read()
    assert content == b"example"


@pytest.fixture(name="temporary_file")
def fixture_temporary_file(tmpdir):
    """Yield a path to a temporary file."""
    foobar = tmpdir.join("foobar.txt")
    foobar.write("hello world")

    yield str(foobar)

    foobar.remove()


def test_file_path(temporary_file):
    """Test loading from a file path."""
    assert load(temporary_file).read() == "hello world"


def test_file_url(temporary_file):
    """Test loading from a file URL."""
    assert load("file://%s" % temporary_file).read() == "hello world"


if __name__ == "__main__":
    mozunit.main()
