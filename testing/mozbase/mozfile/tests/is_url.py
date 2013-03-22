#!/usr/bin/env python

"""
tests for is_url
"""

import unittest
from mozfile import is_url

class TestIsUrl(unittest.TestCase):
    """test the is_url function"""

    def test_is_url(self):
        self.assertTrue(is_url('http://mozilla.org'))
        self.assertFalse(is_url('/usr/bin/mozilla.org'))
        self.assertTrue(is_url('file:///usr/bin/mozilla.org'))
        self.assertFalse(is_url('c:\foo\bar'))

if __name__ == '__main__':
    unittest.main()
