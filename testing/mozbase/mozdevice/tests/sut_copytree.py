#!/usr/bin/env python
# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozdevice
import logging
import unittest

import mozunit

from sut import MockAgent


class CopyTreeTest(unittest.TestCase):

    def test_copyFile(self):
        commands = [('dd if=/mnt/sdcard/tests/test.txt of=/mnt/sdcard/tests/test2.txt', ''),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'test.txt\ntest2.txt')]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=logging.DEBUG)

        self.assertEqual(None, d.copyTree('/mnt/sdcard/tests/test.txt',
                                          '/mnt/sdcard/tests/test2.txt'))
        expected = (commands[3][1].strip()).split('\n')
        self.assertEqual(expected, d.listFiles('/mnt/sdcard/tests'))

    def test_copyDir(self):
        commands = [('dd if=/mnt/sdcard/tests/foo of=/mnt/sdcard/tests/bar', ''),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'foo\nbar')]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port,
                               logLevel=logging.DEBUG)

        self.assertEqual(None, d.copyTree('/mnt/sdcard/tests/foo',
                                          '/mnt/sdcard/tests/bar'))
        expected = (commands[3][1].strip()).split('\n')
        self.assertEqual(expected, d.listFiles('/mnt/sdcard/tests'))

    def test_copyNonEmptyDir(self):
        commands = [('isdir /mnt/sdcard/tests/foo/bar', 'TRUE'),
                    ('dd if=/mnt/sdcard/tests/foo of=/mnt/sdcard/tests/foo2', ''),
                    ('isdir /mnt/sdcard/tests', 'TRUE'),
                    ('cd /mnt/sdcard/tests', ''),
                    ('ls', 'foo\nfoo2'),
                    ('isdir /mnt/sdcard/tests/foo2', 'TRUE'),
                    ('cd /mnt/sdcard/tests/foo2', ''),
                    ('ls', 'bar')]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port,
                               logLevel=logging.DEBUG)

        self.assertTrue(d.dirExists('/mnt/sdcard/tests/foo/bar'))
        self.assertEqual(None, d.copyTree('/mnt/sdcard/tests/foo',
                                          '/mnt/sdcard/tests/foo2'))
        expected = (commands[4][1].strip()).split('\n')
        self.assertEqual(expected, d.listFiles('/mnt/sdcard/tests'))
        self.assertTrue(d.fileExists('/mnt/sdcard/tests/foo2/bar'))

if __name__ == "__main__":
    mozunit.main()
