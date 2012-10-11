from sut import MockAgent
import mozdevice
import unittest

class PsTest(unittest.TestCase):

    pscommands = [('ps',
                   "10029	549	com.android.launcher\n"
                   "10066	1198	com.twitter.android")]

    def test_processList(self):
        a = MockAgent(self,
                      commands=self.pscommands)
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port)
        pslist = d.getProcessList()
        self.assertEqual(len(pslist), 2)
        self.assertEqual(pslist[0], [549, 'com.android.launcher', 10029])
        self.assertEqual(pslist[1], [1198, 'com.twitter.android', 10066])

        a.wait()

    def test_processExist(self):
        for i in [('com.android.launcher', 549),
                  ('com.fennec.android', None)]:
            a = MockAgent(self, commands=self.pscommands)
            d = mozdevice.DroidSUT("127.0.0.1", port=a.port)
            self.assertEqual(d.processExist(i[0]), i[1])
            a.wait()

if __name__ == '__main__':
    unittest.main()
