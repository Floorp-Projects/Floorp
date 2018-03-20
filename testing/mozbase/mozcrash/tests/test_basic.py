#!/usr/bin/env python

from __future__ import absolute_import

import mozunit

import mozcrash
from testcase import CrashTestCase


class TestBasic(CrashTestCase):

    def test_no_dump_files(self):
        """Test that check_for_crashes returns 0 if no dumps are present."""
        self.stdouts.append(["this is some output"])

        self.assertEqual(0, mozcrash.check_for_crashes(self.tempdir,
                                                       symbols_path='symbols_path',
                                                       stackwalk_binary=self.stackwalk,
                                                       quiet=True))

    def test_dump_count(self):
        """Test that check_for_crashes returns True if a dump is present."""
        self.create_minidump("test1")
        self.create_minidump("test2")
        self.create_minidump("test3")

        self.assertEqual(3, mozcrash.check_for_crashes(self.tempdir,
                                                       symbols_path='symbols_path',
                                                       stackwalk_binary=self.stackwalk,
                                                       quiet=True))


if __name__ == '__main__':
    mozunit.main()
