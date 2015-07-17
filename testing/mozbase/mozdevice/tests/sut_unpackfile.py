#!/usr/bin/env python

import mozdevice
import logging
import unittest
from sut import MockAgent


class TestUnpack(unittest.TestCase):

    def test_unpackFile(self):

        commands = [("unzp /data/test/sample.zip /data/test/",
                     "Checksum:          653400271\n"
                     "1 of 1 successfully extracted\n")]
        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=logging.DEBUG)
        # No error being thrown imples all is well
        self.assertEqual(None, d.unpackFile("/data/test/sample.zip",
                                            "/data/test/"))

if __name__ == '__main__':
    unittest.main()
