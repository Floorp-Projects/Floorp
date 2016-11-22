#!/usr/bin/env python
import mozdevice
import logging
import unittest

import mozunit

from sut import MockAgent


class TestGetCurrentTime(unittest.TestCase):

    def test_getCurrentTime(self):
        command = [('clok', '1349980200')]

        m = MockAgent(self, commands=command)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=logging.DEBUG)
        self.assertEqual(d.getCurrentTime(), int(command[0][1]))

if __name__ == '__main__':
    mozunit.main()
