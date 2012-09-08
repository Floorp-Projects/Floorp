#!/usr/bin/env python

import os
import unittest
from manifestparser import TestManifest

here = os.path.dirname(os.path.abspath(__file__))

class TestTestManifest(unittest.TestCase):
    """Test the Test Manifest"""

    def test_testmanifest(self):
        # Test filtering based on platform:
        filter_example = os.path.join(here, 'filter-example.ini')
        manifest = TestManifest(manifests=(filter_example,))
        self.assertEqual([i['name'] for i in manifest.active_tests(os='win', disabled=False, exists=False)],
                         ['windowstest', 'fleem'])
        self.assertEqual([i['name'] for i in manifest.active_tests(os='linux', disabled=False, exists=False)],
                         ['fleem', 'linuxtest'])

        # Look for existing tests.  There is only one:
        self.assertEqual([i['name'] for i in manifest.active_tests()],
                         ['fleem'])

        # You should be able to expect failures:
        last_test = manifest.active_tests(exists=False, toolkit='gtk2')[-1]
        self.assertEqual(last_test['name'], 'linuxtest')
        self.assertEqual(last_test['expected'], 'pass')
        last_test = manifest.active_tests(exists=False, toolkit='cocoa')[-1]
        self.assertEqual(last_test['expected'], 'fail')


if __name__ == '__main__':
    unittest.main()
