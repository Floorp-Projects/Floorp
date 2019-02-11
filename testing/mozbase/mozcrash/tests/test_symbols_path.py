#!/usr/bin/env python
# coding=UTF-8

from __future__ import absolute_import

import zipfile
from six import BytesIO
from six.moves.urllib.parse import urlunsplit

import mozhttpd
import mozunit

from conftest import fspath


def test_symbols_path_not_present(check_for_crashes, minidump_files):
    """Test that no symbols path let mozcrash try to find the symbols."""
    assert 1 == check_for_crashes(symbols_path=None)


def test_symbols_path_unicode(check_for_crashes, minidump_files, tmpdir, capsys):
    """Test that check_for_crashes can handle unicode in dump_directory."""
    symbols_path = tmpdir.mkdir(u"🍪")

    assert 1 == check_for_crashes(symbols_path=fspath(symbols_path),
                                  quiet=False)

    out, _ = capsys.readouterr()
    assert fspath(symbols_path) in out


def test_symbols_path_url(check_for_crashes, minidump_files):
    """Test that passing a URL as symbols_path correctly fetches the URL."""
    data = {"retrieved": False}

    def make_zipfile():
        data = BytesIO()
        z = zipfile.ZipFile(data, 'w')
        z.writestr("symbols.txt", "abc/xyz")
        z.close()
        return data.getvalue()

    def get_symbols(req):
        data["retrieved"] = True

        headers = {}
        return (200, headers, make_zipfile())

    httpd = mozhttpd.MozHttpd(port=0,
                              urlhandlers=[{'method': 'GET',
                                            'path': '/symbols',
                                            'function': get_symbols}])
    httpd.start()
    symbol_url = urlunsplit(('http', '%s:%d' % httpd.httpd.server_address,
                             '/symbols', '', ''))

    assert 1 == check_for_crashes(symbols_path=symbol_url)
    assert data["retrieved"]


if __name__ == '__main__':
    mozunit.main()
