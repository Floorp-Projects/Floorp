#!/usr/bin/env python
import mozdevice
import logging
import unittest

import mozunit

from sut import MockAgent


class TestListFiles(unittest.TestCase):
    commands = [("isdir /mnt/sdcard", "TRUE"),
                ("cd /mnt/sdcard", ""),
                ("ls", "Android\nMusic\nPodcasts\nRingtones\nAlarms\n"
                       "Notifications\nPictures\nMovies\nDownload\nDCIM\n")]

    def test_listFiles(self):
        m = MockAgent(self, commands=self.commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=logging.DEBUG)

        expected = (self.commands[2][1].strip()).split("\n")
        self.assertEqual(expected, d.listFiles("/mnt/sdcard"))

if __name__ == '__main__':
    mozunit.main()
