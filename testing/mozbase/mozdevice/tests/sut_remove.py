#!/usr/bin/env python
import mozdevice
import logging
import unittest

import mozunit

from sut import MockAgent


class TestRemove(unittest.TestCase):

    def test_removeDir(self):
        commands = [("isdir /mnt/sdcard/test", "TRUE"),
                    ("rmdr /mnt/sdcard/test", "Deleting file(s) from "
                     "/storage/emulated/legacy/Moztest\n"
                     "        <empty>\n"
                     "Deleting directory "
                     "/storage/emulated/legacy/Moztest\n")]

        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=logging.DEBUG)
        # No error implies we're all good
        self.assertEqual(None, d.removeDir("/mnt/sdcard/test"))

if __name__ == '__main__':
    mozunit.main()
