#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozdevice
import mozlog
import unittest
from sut import MockAgent

class MoveTreeTest(unittest.TestCase):
    def test_moveFile(self):
        commands = [('mv /mnt/sdcard/tests/test.txt /mnt/sdcard/tests/test1.txt', ''),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'test1.txt'),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'test1.txt')]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertEqual(None, d.moveTree('/mnt/sdcard/tests/test.txt',
                '/mnt/sdcard/tests/test1.txt'))
        self.assertFalse(d.fileExists('/mnt/sdcard/tests/test.txt'))
        self.assertTrue(d.fileExists('/mnt/sdcard/tests/test1.txt'))

    def test_moveDir(self):
        commands = [("mv /mnt/sdcard/tests/foo /mnt/sdcard/tests/bar", ""),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'bar')]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertEqual(None, d.moveTree('/mnt/sdcard/tests/foo',
                '/mnt/sdcard/tests/bar'))
        self.assertTrue(d.fileExists('/mnt/sdcard/tests/bar'))

    def test_moveNonEmptyDir(self):
        commands = [('isdir /mnt/sdcard/tests/foo/bar', 'TRUE'),
                    ('mv /mnt/sdcard/tests/foo /mnt/sdcard/tests/foo2', ''),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'foo2'),
                    ('isdir /mnt/sdcard/tests/foo2', 'TRUE'),
                    ('cd /mnt/sdcard/tests/foo2', ''),
                    ('ls', 'bar')]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port,
                logLevel=mozlog.DEBUG)

        self.assertTrue(d.dirExists('/mnt/sdcard/tests/foo/bar'))
        self.assertEqual(None, d.moveTree('/mnt/sdcard/tests/foo',
                '/mnt/sdcard/tests/foo2'))
        self.assertTrue(d.fileExists('/mnt/sdcard/tests/foo2'))
        self.assertTrue(d.fileExists('/mnt/sdcard/tests/foo2/bar'))

if __name__ == "__main__":
    unittest.main()
