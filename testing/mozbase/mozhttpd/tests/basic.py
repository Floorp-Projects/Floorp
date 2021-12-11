#!/usr/bin/env python

from __future__ import absolute_import

import mozhttpd
import mozfile
import os

import pytest

import mozunit


@pytest.fixture(name="files")
def fixture_files():
    """Return a list of tuples with name and binary_string."""
    return [("small", os.urandom(128)), ("large", os.urandom(16384))]


@pytest.fixture(name="docroot")
def fixture_docroot(tmpdir, files):
    """Yield a str path to docroot."""
    docroot = tmpdir.mkdir("docroot")

    for name, binary_string in files:
        filename = docroot.join(name)
        filename.write_binary(binary_string)

    yield str(docroot)

    docroot.remove()


@pytest.fixture(name="httpd_url")
def fixture_httpd_url(docroot):
    """Yield the URL to a started MozHttpd server."""
    httpd = mozhttpd.MozHttpd(docroot=docroot)
    httpd.start()
    yield httpd.get_url()
    httpd.stop()


def test_basic(httpd_url, files):
    """Test that mozhttpd can serve files."""

    # Retrieve file and check contents matchup
    for name, binary_string in files:
        retrieved_content = mozfile.load(httpd_url + name).read()
        assert retrieved_content == binary_string


if __name__ == "__main__":
    mozunit.main()
