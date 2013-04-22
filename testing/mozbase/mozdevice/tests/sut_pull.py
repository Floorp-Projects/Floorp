from sut import MockAgent
import mozdevice
import mozlog
import unittest
import hashlib
import tempfile
import os

class PullTest(unittest.TestCase):

    def test_pull_success(self):
        for count in [ 1, 4, 1024, 2048 ]:
            cheeseburgers = ""
            for i in range(count):
                cheeseburgers += "cheeseburgers"

            # pull file is kind of gross, make sure we can still execute commands after it's done
            remoteName = "/mnt/sdcard/cheeseburgers"
            a = MockAgent(self, commands = [("pull %s" % remoteName,
                                             "%s,%s\n%s" % (remoteName,
                                                            len(cheeseburgers),
                                                            cheeseburgers)),
                                            ("isdir /mnt/sdcard", "TRUE")])

            d = mozdevice.DroidSUT("127.0.0.1", port=a.port,
                                   logLevel=mozlog.DEBUG)
            pulledData = d.pullFile("/mnt/sdcard/cheeseburgers")
            self.assertEqual(pulledData, cheeseburgers)
            d.dirExists('/mnt/sdcard')

    def test_pull_failure(self):

        # this test simulates only receiving a few bytes of what we expect
        # to be larger file
        remoteName = "/mnt/sdcard/cheeseburgers"
        a = MockAgent(self, commands = [("pull %s" % remoteName,
                                         "%s,15\n%s" % (remoteName,
                                                        "cheeseburgh"))])
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port,
                               logLevel=mozlog.DEBUG)
        exceptionThrown = False
        try:
            d.pullFile("/mnt/sdcard/cheeseburgers")
        except mozdevice.DMError:
            exceptionThrown = True
        self.assertTrue(exceptionThrown)

if __name__ == '__main__':
    unittest.main()


