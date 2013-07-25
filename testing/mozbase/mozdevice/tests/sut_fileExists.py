from sut import MockAgent
import mozdevice
import unittest

class FileExistsTest(unittest.TestCase):

    commands = [('isdir /', 'TRUE'),
                ('cd /', ''),
                ('ls', 'init')]

    def test_onRoot(self):
        root_commands = [('isdir /', 'TRUE')]
        a = MockAgent(self, commands=root_commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port)
        self.assertTrue(d.fileExists('/'))

    def test_onNonexistent(self):
        a = MockAgent(self, commands=self.commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port)
        self.assertFalse(d.fileExists('/doesNotExist'))

    def test_onRegularFile(self):
        a = MockAgent(self, commands=self.commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=a.port)
        self.assertTrue(d.fileExists('/init'))

if __name__ == '__main__':
    unittest.main()

