#/usr/bin/env python
import mozdevice
import mozlog
import unittest
from sut import MockAgent


class TestApp(unittest.TestCase):

    def test_getAppRoot(self):
        command = [("getapproot org.mozilla.firefox",
                    "/data/data/org.mozilla.firefox")]

        m = MockAgent(self, commands=command)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)

        self.assertEqual(command[0][1], d.getAppRoot('org.mozilla.firefox'))

if __name__ == '__main__':
    unittest.main()
