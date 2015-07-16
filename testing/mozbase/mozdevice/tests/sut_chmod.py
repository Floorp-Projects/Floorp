#/usr/bin/env python
import mozdevice
import logging
import unittest
from sut import MockAgent


class TestChmod(unittest.TestCase):

    def test_chmod(self):

        command = [('chmod /mnt/sdcard/test', 'Changing permissions for /storage/emulated/legacy/Test\n'
                                              '        <empty>\n'
                                              'chmod /storage/emulated/legacy/Test ok\n')]
        m = MockAgent(self, commands=command)
        d = mozdevice.DroidSUT('127.0.0.1', port=m.port, logLevel=logging.DEBUG)

        self.assertEqual(None, d.chmodDir('/mnt/sdcard/test'))

if __name__ == '__main__':
    unittest.main()
