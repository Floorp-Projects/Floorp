#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
test URL parsing; see
https://bugzilla.mozilla.org/show_bug.cgi?id=793875
"""

import unittest
import talos.utils


class TestURLParsing(unittest.TestCase):

    def test_http_url(self):
        """test parsing an HTTP URL"""

        url = 'https://www.mozilla.org/en-US/about/'
        parsed = talos.utils.urlsplit(url)
        self.assertEqual(parsed,
                         ['https', 'www.mozilla.org', '/en-US/about/', '', ''])

    def test_file_url(self):
        """test parsing file:// URLs"""

        # unix-like file path
        url = 'file:///foo/bar'
        parsed = talos.utils.urlsplit(url)
        self.assertEqual(parsed,
                         ['file', '', '/foo/bar', '', ''])

        # windows-like file path
        url = r'file://c:\foo\bar'
        parsed = talos.utils.urlsplit(url)
        self.assertEqual(parsed,
                         ['file', '', r'c:\foo\bar', '', ''])

    def test_implicit_file_url(self):
        """
        test parsing URLs with no scheme, which by default are assumed
        to be file:// URLs
        """

        path = '/foo/bar'
        parsed = talos.utils.urlsplit(path)
        self.assertEqual(parsed,
                         ['file', '', '/foo/bar', '', ''])


if __name__ == '__main__':
    unittest.main()
