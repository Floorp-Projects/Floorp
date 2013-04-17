from sut import MockAgent
import mozdevice
import mozlog
import unittest

class LaunchTest(unittest.TestCase):

    def test_nouserserial(self):
        a = MockAgent(self, commands = [("ps",
                                         "10029	549	com.android.launcher\n"
                                         "10066	1198	com.twitter.android"),
                                        ("info sutuserinfo", ""),
                                        ("exec am start -W -n "
                                         "org.mozilla.fennec/.App -a "
                                         "android.intent.action.VIEW",
                                         "OK\nreturn code [0]")])
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port, logLevel=mozlog.DEBUG)
        d.launchFennec("org.mozilla.fennec")
        a.wait()

    def test_userserial(self):
        a = MockAgent(self, commands = [("ps",
                                         "10029	549	com.android.launcher\n"
                                         "10066	1198	com.twitter.android"),
                                        ("info sutuserinfo", "User Serial:0"),
                                        ("exec am start --user 0 -W -n "
                                         "org.mozilla.fennec/.App -a "
                                         "android.intent.action.VIEW",
                                         "OK\nreturn code [0]")])
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port, logLevel=mozlog.DEBUG)
        d.launchFennec("org.mozilla.fennec")
        a.wait()

if __name__ == '__main__':
    unittest.main()
