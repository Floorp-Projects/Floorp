#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit

import mozcrash
from testcase import CrashTestCase


class TestSavePath(CrashTestCase):

    def setUp(self):
        super(TestSavePath, self).setUp()

        self.create_minidump("test")

    def check_for_saved_minidump_files(self, save_path=None):
        self.assertEqual(1, mozcrash.check_for_crashes(self.tempdir,
                                                       symbols_path='symbols_path',
                                                       stackwalk_binary=self.stackwalk,
                                                       dump_save_path=save_path,
                                                       quiet=True))
        if save_path is None:
            save_path = os.environ.get('MINIDUMP_SAVE_PATH', None)

        self.assert_(os.path.isfile(os.path.join(save_path, "test.dmp")))
        self.assert_(os.path.isfile(os.path.join(save_path, "test.extra")))

    def test_save_path_not_present(self):
        """Test that dump_save_path works when the directory doesn't exist."""
        save_path = os.path.join(self.tempdir, "saved")

        self.check_for_saved_minidump_files(save_path)

    def test_save_path(self):
        """Test that dump_save_path works."""
        save_path = os.path.join(self.tempdir, "saved")
        os.mkdir(save_path)

        self.check_for_saved_minidump_files(save_path)

    def test_save_path_isfile(self):
        """Test that dump_save_path works when the path is a file and not a directory."""
        save_path = os.path.join(self.tempdir, "saved")
        open(save_path, "w").write("junk")

        self.check_for_saved_minidump_files(save_path)

    def test_save_path_envvar(self):
        """Test that the MINDUMP_SAVE_PATH environment variable works."""
        save_path = os.path.join(self.tempdir, "saved")
        os.mkdir(save_path)

        os.environ['MINIDUMP_SAVE_PATH'] = save_path
        try:
            self.check_for_saved_minidump_files()
        finally:
            del os.environ['MINIDUMP_SAVE_PATH']


if __name__ == '__main__':
    mozunit.main()
