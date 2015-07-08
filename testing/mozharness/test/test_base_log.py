import os
import shutil
import subprocess
import unittest

import mozharness.base.log as log

tmp_dir = "test_log_dir"
log_name = "test"


def clean_log_dir():
    if os.path.exists(tmp_dir):
        shutil.rmtree(tmp_dir)


def get_log_file_path(level=None):
    if level:
        return os.path.join(tmp_dir, "%s_%s.log" % (log_name, level))
    return os.path.join(tmp_dir, "%s.log" % log_name)


class TestLog(unittest.TestCase):
    def setUp(self):
        clean_log_dir()

    def tearDown(self):
        clean_log_dir()

    def test_log_dir(self):
        fh = open(tmp_dir, 'w')
        fh.write("foo")
        fh.close()
        l = log.SimpleFileLogger(log_dir=tmp_dir, log_name=log_name,
                                 log_to_console=False)
        self.assertTrue(os.path.exists(tmp_dir))
        l.log_message('blah')
        self.assertTrue(os.path.exists(get_log_file_path()))
        del(l)

if __name__ == '__main__':
    unittest.main()
