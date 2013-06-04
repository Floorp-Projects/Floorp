#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
tests for mozfile.NamedTemporaryFile
"""

import mozfile
import os
import unittest

class TestNamedTemporaryFile(unittest.TestCase):

    def test_iteration(self):
        """ensure the line iterator works"""

        # make a file and write to it
        tf = mozfile.NamedTemporaryFile()
        notes = ['doe', 'rae', 'mi']
        for note in notes:
            tf.write('%s\n' % note)
        tf.flush()

        # now read from it
        tf.seek(0)
        lines = [line.rstrip('\n') for line in tf.readlines()]
        self.assertEqual(lines, notes)

        # now read from it iteratively
        lines = []
        for line in tf:
            lines.append(line.strip())
        self.assertEqual(lines, []) # because we did not seek(0)
        tf.seek(0)
        lines = []
        for line in tf:
            lines.append(line.strip())
        self.assertEqual(lines, notes)

    def test_delete(self):
        """ensure ``delete=True/False`` works as expected"""

        # make a deleteable file; ensure it gets cleaned up
        path = None
        with mozfile.NamedTemporaryFile(delete=True) as tf:
            path = tf.name
        self.assertTrue(isinstance(path, basestring))
        self.assertFalse(os.path.exists(path))

        # it is also deleted when __del__ is called
        # here we will do so explicitly
        tf = mozfile.NamedTemporaryFile(delete=True)
        path = tf.name
        self.assertTrue(os.path.exists(path))
        del tf
        self.assertFalse(os.path.exists(path))

        # Now the same thing but we won't delete the file
        path = None
        try:
            with mozfile.NamedTemporaryFile(delete=False) as tf:
                path = tf.name
            self.assertTrue(os.path.exists(path))
        finally:
            if path and os.path.exists(path):
                os.remove(path)

        path = None
        try:
            tf = mozfile.NamedTemporaryFile(delete=False)
            path = tf.name
            self.assertTrue(os.path.exists(path))
            del tf
            self.assertTrue(os.path.exists(path))
        finally:
            if path and os.path.exists(path):
                os.remove(path)

if __name__ == '__main__':
    unittest.main()
