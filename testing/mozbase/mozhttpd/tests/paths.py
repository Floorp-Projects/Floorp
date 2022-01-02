#!/usr/bin/env python

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

from six.moves.urllib.request import urlopen
from six.moves.urllib.error import HTTPError

import pytest

import mozunit
import mozhttpd


def try_get(url, expected_contents):
    f = urlopen(url)
    assert f.getcode() == 200
    assert f.read() == expected_contents


def try_get_expect_404(url):
    with pytest.raises(HTTPError) as excinfo:
        urlopen(url)
    assert excinfo.value.code == 404


@pytest.fixture(name="httpd_basic")
def fixture_httpd_basic(tmpdir):
    d1 = tmpdir.mkdir("d1")
    d1.join("test1.txt").write("test 1 contents")

    d2 = tmpdir.mkdir("d2")
    d2.join("test2.txt").write("test 2 contents")

    httpd = mozhttpd.MozHttpd(
        port=0,
        docroot=str(d1),
        path_mappings={"/files": str(d2)},
    )
    httpd.start(block=False)

    yield httpd

    httpd.stop()
    d1.remove()
    d2.remove()


def test_basic(httpd_basic):
    """Test that requests to docroot and a path mapping work as expected."""
    try_get(httpd_basic.get_url("/test1.txt"), b"test 1 contents")
    try_get(httpd_basic.get_url("/files/test2.txt"), b"test 2 contents")
    try_get_expect_404(httpd_basic.get_url("/files/test2_nope.txt"))


@pytest.fixture(name="httpd_substring_mappings")
def fixture_httpd_substring_mappings(tmpdir):
    d1 = tmpdir.mkdir("d1")
    d1.join("test1.txt").write("test 1 contents")

    d2 = tmpdir.mkdir("d2")
    d2.join("test2.txt").write("test 2 contents")

    httpd = mozhttpd.MozHttpd(
        port=0,
        path_mappings={"/abcxyz": str(d1), "/abc": str(d2)},
    )
    httpd.start(block=False)
    yield httpd
    httpd.stop()
    d1.remove()
    d2.remove()


def test_substring_mappings(httpd_substring_mappings):
    httpd = httpd_substring_mappings
    try_get(httpd.get_url("/abcxyz/test1.txt"), b"test 1 contents")
    try_get(httpd.get_url("/abc/test2.txt"), b"test 2 contents")


@pytest.fixture(name="httpd_multipart_path_mapping")
def fixture_httpd_multipart_path_mapping(tmpdir):
    d1 = tmpdir.mkdir("d1")
    d1.join("test1.txt").write("test 1 contents")

    httpd = mozhttpd.MozHttpd(
        port=0,
        path_mappings={"/abc/def/ghi": str(d1)},
    )
    httpd.start(block=False)
    yield httpd
    httpd.stop()
    d1.remove()


def test_multipart_path_mapping(httpd_multipart_path_mapping):
    """Test that a path mapping with multiple directories works."""
    httpd = httpd_multipart_path_mapping
    try_get(httpd.get_url("/abc/def/ghi/test1.txt"), b"test 1 contents")
    try_get_expect_404(httpd.get_url("/abc/test1.txt"))
    try_get_expect_404(httpd.get_url("/abc/def/test1.txt"))


@pytest.fixture(name="httpd_no_docroot")
def fixture_httpd_no_docroot(tmpdir):
    d1 = tmpdir.mkdir("d1")
    httpd = mozhttpd.MozHttpd(
        port=0,
        path_mappings={"/foo": str(d1)},
    )
    httpd.start(block=False)
    yield httpd
    httpd.stop()
    d1.remove()


def test_no_docroot(httpd_no_docroot):
    """Test that path mappings with no docroot work."""
    try_get_expect_404(httpd_no_docroot.get_url())


if __name__ == "__main__":
    mozunit.main()
