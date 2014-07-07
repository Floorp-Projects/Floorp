#!/usr/bin/env python

import hashlib
import mozdevice
import mozlog
import shutil
import tempfile
import unittest
from sut import MockAgent


class TestFileMethods(unittest.TestCase):
    """ Class to test misc file methods """

    content = "What is the answer to the life, universe and everything? 42"
    h = hashlib.md5()
    h.update(content)
    temp_hash = h.hexdigest()

    def test_validateFile(self):

        with tempfile.NamedTemporaryFile() as f:
            f.write(self.content)
            f.flush()

            # Test Valid Hashes
            commands_valid = [("hash /sdcard/test/file", self.temp_hash)]

            m = MockAgent(self, commands=commands_valid)
            d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
            self.assertTrue(d.validateFile('/sdcard/test/file', f.name))

            # Test invalid hashes
            commands_invalid = [("hash /sdcard/test/file", "0this0hash0is0completely0invalid")]

            m = MockAgent(self, commands=commands_invalid)
            d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
            self.assertFalse(d.validateFile('/sdcard/test/file', f.name))

    def test_getFile(self):

        fname = "/mnt/sdcard/file"
        commands = [("pull %s" % fname, "%s,%s\n%s" % (fname, len(self.content), self.content)),
                    ("hash %s" % fname, self.temp_hash)]

        with tempfile.NamedTemporaryFile() as f:
            m = MockAgent(self, commands=commands)
            d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
            # No error means success
            self.assertEqual(None, d.getFile(fname, f.name))

    def test_getDirectory(self):

        fname = "/mnt/sdcard/file"
        commands = [("isdir /mnt/sdcard", "TRUE"),
                    ("isdir /mnt/sdcard", "TRUE"),
                    ("cd /mnt/sdcard", ""),
                    ("ls", "file"),
                    ("isdir %s" % fname, "FALSE"),
                    ("pull %s" % fname, "%s,%s\n%s" % (fname, len(self.content), self.content)),
                    ("hash %s" % fname, self.temp_hash)]

        tmpdir = tempfile.mkdtemp()
        m = MockAgent(self, commands=commands)
        d = mozdevice.DroidSUT("127.0.0.1", port=m.port, logLevel=mozlog.DEBUG)
        self.assertEqual(None, d.getDirectory("/mnt/sdcard", tmpdir))

        # Cleanup
        shutil.rmtree(tmpdir)

if __name__ == '__main__':
    unittest.main()
