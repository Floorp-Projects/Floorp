#!/usr/bin/env python

from __future__ import absolute_import

import urlparse
import zipfile
import StringIO

import mozhttpd
import mozunit


def test_symbols_path_not_present(check_for_crashes, minidump_files):
    """Test that no symbols path let mozcrash try to find the symbols."""
    assert 1 == check_for_crashes(symbols_path=None)


def test_symbols_path_url(check_for_crashes, minidump_files):
    """Test that passing a URL as symbols_path correctly fetches the URL."""
    data = {"retrieved": False}

    def make_zipfile():
        data = StringIO.StringIO()
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
    symbol_url = urlparse.urlunsplit(('http', '%s:%d' % httpd.httpd.server_address,
                                      '/symbols', '', ''))

    assert 1 == check_for_crashes(symbols_path=symbol_url)
    assert data["retrieved"]


if __name__ == '__main__':
    mozunit.main()
