# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

import mozdevice
import mozlog
import unittest
from sut import MockAgent

class MkDirsTest(unittest.TestCase):

    def test_mkdirs(self):
        subTests = [{'cmds': [('isdir /mnt/sdcard/baz/boop', 'FALSE'),
                              ('isdir /mnt', 'TRUE'),
                              ('isdir /mnt/sdcard', 'TRUE'),
                              ('isdir /mnt/sdcard/baz', 'FALSE'),
                              ('mkdr /mnt/sdcard/baz',
                               '/mnt/sdcard/baz successfully created'),
                              ('isdir /mnt/sdcard/baz/boop', 'FALSE'),
                              ('mkdr /mnt/sdcard/baz/boop',
                               '/mnt/sdcard/baz/boop successfully created')],
                     'expectException': False},
                    {'cmds': [('isdir /mnt/sdcard/baz/boop', 'FALSE'),
                              ('isdir /mnt', 'TRUE'),
                              ('isdir /mnt/sdcard', 'TRUE'),
                              ('isdir /mnt/sdcard/baz', 'FALSE'),
                              ('mkdr /mnt/sdcard/baz',
                               '##AGENT-WARNING## Could not create the directory /mnt/sdcard/baz')],
                     'expectException': True},
                     ]
        for subTest in subTests:
            a = MockAgent(self, commands=subTest['cmds'])

            exceptionThrown = False
            try:
                d = mozdevice.DroidSUT('127.0.0.1', port=a.port,
                                       logLevel=mozlog.DEBUG)
                d.mkDirs('/mnt/sdcard/baz/boop/bip')
            except mozdevice.DMError:
                exceptionThrown = True
            self.assertEqual(exceptionThrown, subTest['expectException'])

            a.wait()

    def test_repeated_path_part(self):
        """
        Ensure that all dirs are created when last path part also found
        earlier in the path (bug 826492).
        """

        cmds = [('isdir /mnt/sdcard/foo', 'FALSE'),
                ('isdir /mnt', 'TRUE'),
                ('isdir /mnt/sdcard', 'TRUE'),
                ('isdir /mnt/sdcard/foo', 'FALSE'),
                ('mkdr /mnt/sdcard/foo',
                 '/mnt/sdcard/foo successfully created')]
        a = MockAgent(self, commands=cmds)
        d = mozdevice.DroidSUT('127.0.0.1', port=a.port,
                               logLevel=mozlog.DEBUG)
        d.mkDirs('/mnt/sdcard/foo/foo')
        a.wait()


if __name__ == '__main__':
    unittest.main()
