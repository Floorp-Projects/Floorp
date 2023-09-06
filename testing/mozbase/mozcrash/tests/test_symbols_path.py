#!/usr/bin/env python
# coding=UTF-8

import zipfile

import mozhttpd
import mozunit
from conftest import fspath
from six import BytesIO
from six.moves.urllib.parse import urlunsplit


def test_symbols_path_not_present(check_for_crashes, minidump_files):
    """Test that no symbols path let mozcrash try to find the symbols."""
    assert 1 == check_for_crashes(symbols_path=None)


def test_symbols_path_unicode(check_for_crashes, minidump_files, tmpdir, capsys):
    """Test that check_for_crashes can handle unicode in dump_directory."""
    symbols_path = tmpdir.mkdir("🍪")

    assert 1 == check_for_crashes(symbols_path=fspath(symbols_path), quiet=False)

    out, _ = capsys.readouterr()
    assert fspath(symbols_path) in out


def test_symbols_path_url(check_for_crashes, minidump_files):
    """Test that passing a URL as symbols_path correctly fetches the URL."""
    data = {"retrieved": False}

    def make_zipfile():
        zdata = BytesIO()
        z = zipfile.ZipFile(zdata, "w")
        z.writestr("symbols.txt", "abc/xyz")
        z.close()
        return zdata.getvalue()

    def get_symbols(req):
        data["retrieved"] = True

        headers = {}
        return (200, headers, make_zipfile())

    httpd = mozhttpd.MozHttpd(
        port=0,
        urlhandlers=[{"method": "GET", "path": "/symbols", "function": get_symbols}],
    )
    httpd.start()
    symbol_url = urlunsplit(
        ("http", "%s:%d" % httpd.httpd.server_address, "/symbols", "", "")
    )

    assert 1 == check_for_crashes(symbols_path=symbol_url)
    assert data["retrieved"]


def test_symbols_retry(check_for_crashes, minidump_files):
    """Test that passing a URL as symbols_path succeeds on retry after temporary HTTP failure."""
    data = {"retrieved": False}
    get_symbols_calls = 0

    def make_zipfile():
        zdata = BytesIO()
        z = zipfile.ZipFile(zdata, "w")
        z.writestr("symbols.txt", "abc/xyz")
        z.close()
        return zdata.getvalue()

    def get_symbols(req):
        nonlocal get_symbols_calls
        data["retrieved"] = True
        if get_symbols_calls > 0:
            ret = 200
        else:
            ret = 504
        get_symbols_calls += 1

        headers = {}
        return (ret, headers, make_zipfile())

    httpd = mozhttpd.MozHttpd(
        port=0,
        urlhandlers=[{"method": "GET", "path": "/symbols", "function": get_symbols}],
    )
    httpd.start()
    symbol_url = urlunsplit(
        ("http", "%s:%d" % httpd.httpd.server_address, "/symbols", "", "")
    )

    assert 1 == check_for_crashes(symbols_path=symbol_url)
    assert data["retrieved"]
    assert 2 == get_symbols_calls


if __name__ == "__main__":
    mozunit.main()
