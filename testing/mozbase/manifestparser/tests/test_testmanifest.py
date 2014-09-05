#!/usr/bin/env python

import os
import shutil
import tempfile
import unittest
from manifestparser import TestManifest

here = os.path.dirname(os.path.abspath(__file__))

class TestTestManifest(unittest.TestCase):
    """Test the Test Manifest"""

    def test_testmanifest(self):
        # Test filtering based on platform:
        filter_example = os.path.join(here, 'filter-example.ini')
        manifest = TestManifest(manifests=(filter_example,), strict=False)
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

    def test_missing_paths(self):
        """
        Test paths that don't exist raise an exception in strict mode.
        """
        tempdir = tempfile.mkdtemp()

        missing_path = os.path.join(here, 'missing-path.ini')
        manifest = TestManifest(manifests=(missing_path,), strict=True)
        self.assertRaises(IOError, manifest.active_tests)
        self.assertRaises(IOError, manifest.copy, tempdir)
        self.assertRaises(IOError, manifest.update, tempdir)

        shutil.rmtree(tempdir)


    def test_comments(self):
        """
        ensure comments work, see
        https://bugzilla.mozilla.org/show_bug.cgi?id=813674
        """
        comment_example = os.path.join(here, 'comment-example.ini')
        manifest = TestManifest(manifests=(comment_example,))
        self.assertEqual(len(manifest.tests), 8)
        names = [i['name'] for i in manifest.tests]
        self.assertFalse('test_0202_app_launch_apply_update_dirlocked.js' in names)


if __name__ == '__main__':
    unittest.main()
