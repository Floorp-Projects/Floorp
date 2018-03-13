#!/usr/bin/env python

from __future__ import absolute_import

import mozunit

import mozcrash
from testcase import CrashTestCase


class TestBasic(CrashTestCase):

    def test_nodumps(self):
        """Test that check_for_crashes returns False if no dumps are present."""
        self.stdouts.append(["this is some output"])
        self.assertFalse(mozcrash.check_for_crashes(self.tempdir,
                                                    symbols_path='symbols_path',
                                                    stackwalk_binary=self.stackwalk,
                                                    quiet=True))

    def test_simple(self):
        """Test that check_for_crashes returns True if a dump is present."""
        self.create_minidump("test")

        self.assert_(mozcrash.check_for_crashes(self.tempdir,
                                                symbols_path='symbols_path',
                                                stackwalk_binary=self.stackwalk,
                                                quiet=True))


if __name__ == '__main__':
    mozunit.main()
