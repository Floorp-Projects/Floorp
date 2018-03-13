#!/usr/bin/env python

from __future__ import absolute_import

import urlparse
import zipfile
import StringIO

import mozunit

import mozcrash
import mozhttpd

from testcase import CrashTestCase


class TestCrash(CrashTestCase):

    def test_symbol_path_not_present(self):
        """Test that no symbols path doesn't process the minidump."""
        self.create_minidump("test")

        self.assert_(mozcrash.check_for_crashes(self.tempdir,
                                                symbols_path=None,
                                                stackwalk_binary=self.stackwalk,
                                                quiet=True))

    def test_symbol_path_url(self):
        """Test that passing a URL as symbols_path correctly fetches the URL."""
        self.create_minidump("test")

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

        self.assert_(mozcrash.check_for_crashes(self.tempdir,
                                                symbols_path=symbol_url,
                                                stackwalk_binary=self.stackwalk,
                                                quiet=True))
        self.assertTrue(data["retrieved"])


if __name__ == '__main__':
    mozunit.main()
