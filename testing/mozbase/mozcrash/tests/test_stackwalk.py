#!/usr/bin/env python

from __future__ import absolute_import

import os

import mozunit

import mozcrash
from testcase import CrashTestCase


class TestStackwalk(CrashTestCase):

    def test_stackwalk_envvar(self):
        """Test that check_for_crashes uses the MINIDUMP_STACKWALK environment var."""
        self.create_minidump("test.")

        os.environ['MINIDUMP_STACKWALK'] = self.stackwalk
        self.assert_(mozcrash.check_for_crashes(self.tempdir,
                                                symbols_path='symbols_path',
                                                quiet=True))
        del os.environ['MINIDUMP_STACKWALK']


if __name__ == '__main__':
    mozunit.main()
