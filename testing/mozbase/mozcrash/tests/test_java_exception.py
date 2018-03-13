#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import unittest

import mozlog.unstructured as mozlog
import mozunit

import mozcrash


# Make logs go away
try:
    log = mozlog.getLogger("mozcrash", handler=mozlog.FileHandler(os.devnull))
except ValueError:
    pass


class TestJavaException(unittest.TestCase):

    def setUp(self):
        self.test_log = [
            "01-30 20:15:41.937 E/GeckoAppShell( 1703): >>> "
            "REPORTING UNCAUGHT EXCEPTION FROM THREAD 9 (\"GeckoBackgroundThread\")",
            "01-30 20:15:41.937 E/GeckoAppShell( 1703): java.lang.NullPointerException",
            "01-30 20:15:41.937 E/GeckoAppShell( 1703):"
            "    at org.mozilla.gecko.GeckoApp$21.run(GeckoApp.java:1833)",
            "01-30 20:15:41.937 E/GeckoAppShell( 1703):"
            "    at android.os.Handler.handleCallback(Handler.java:587)"]

    def test_uncaught_exception(self):
        """
        Test for an exception which should be caught
        """
        self.assert_(mozcrash.check_for_java_exception(self.test_log, quiet=True))

    def test_truncated_exception(self):
        """
        Test for an exception which should be caught which
        was truncated
        """
        truncated_log = list(self.test_log)
        truncated_log[0], truncated_log[1] = truncated_log[1], truncated_log[0]
        self.assert_(mozcrash.check_for_java_exception(truncated_log, quiet=True))

    def test_unchecked_exception(self):
        """
        Test for an exception which should not be caught
        """
        passable_log = list(self.test_log)
        passable_log[0] = "01-30 20:15:41.937 E/GeckoAppShell( 1703):" \
                          " >>> NOT-SO-BAD EXCEPTION FROM THREAD 9 (\"GeckoBackgroundThread\")"
        self.assert_(not mozcrash.check_for_java_exception(passable_log, quiet=True))


if __name__ == '__main__':
    mozunit.main()
