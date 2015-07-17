#!/usr/bin/env python

import mozdevice
import logging
import unittest
from sut import MockAgent


class TestKill(unittest.TestCase):

    def test_killprocess(self):
        commands = [("ps", "1000    1486    com.android.settings\n"
                           "10016   420 com.android.location.fused\n"
                           "10023   335 com.android.systemui\n"),
                    ("kill com.android.settings",
                     "Successfully killed com.android.settings\n")]
        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=logging.DEBUG)
        # No error raised means success
        self.assertEqual(None,  d.killProcess("com.android.settings"))


if __name__ == '__main__':
    unittest.main()
