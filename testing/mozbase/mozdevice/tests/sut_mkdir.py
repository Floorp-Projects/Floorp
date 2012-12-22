from sut import MockAgent
import mozdevice
import unittest

class PushTest(unittest.TestCase):

    def test_mkdirs(self):
        subTests = [ { 'cmds': [ ("isdir /mnt/sdcard/baz/boop", "FALSE"),
                                 ("isdir /mnt", "TRUE"),
                                 ("isdir /mnt/sdcard", "TRUE"),
                                 ("isdir /mnt/sdcard/baz", "FALSE"),
                                 ("mkdr /mnt/sdcard/baz",
                                  "/mnt/sdcard/baz successfully created"),
                                 ("isdir /mnt/sdcard/baz/boop", "FALSE"),
                                 ("mkdr /mnt/sdcard/baz/boop",
                                  "/mnt/sdcard/baz/boop successfully created") ],
                       'expectException': False },
                     { 'cmds': [ ("isdir /mnt/sdcard/baz/boop", "FALSE"),
                                 ("isdir /mnt", "TRUE"),
                                 ("isdir /mnt/sdcard", "TRUE"),
                                 ("isdir /mnt/sdcard/baz", "FALSE"),
                                 ("mkdr /mnt/sdcard/baz",
                                  "##AGENT-WARNING## Could not create the directory /mnt/sdcard/baz") ],
                       'expectException': True },
                     ]
        for subTest in subTests:
            a = MockAgent(self, commands = subTest['cmds'])

            exceptionThrown = False
            try:
                mozdevice.DroidSUT.debug = 4
                d = mozdevice.DroidSUT("127.0.0.1", port=a.port)
                d.mkDirs("/mnt/sdcard/baz/boop/bip")
            except mozdevice.DMError, e:
                exceptionThrown = True
            self.assertEqual(exceptionThrown, subTest['expectException'])

            a.wait()

if __name__ == '__main__':
    unittest.main()
