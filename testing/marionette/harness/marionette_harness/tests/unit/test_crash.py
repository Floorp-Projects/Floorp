# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import shutil

from marionette_driver.errors import MarionetteException
# Import runner module to monkey patch mozcrash module
from mozrunner.base import runner

from marionette_harness import MarionetteTestCase, expectedFailure, run_if_e10s


class MockMozCrash(object):
    """Mock object to replace original mozcrash methods."""

    def __init__(self, marionette):
        self.marionette = marionette

        with self.marionette.using_context('chrome'):
            self.crash_reporter_enabled = self.marionette.execute_script("""
                try {
                  Components.classes["@mozilla.org/toolkit/crash-reporter;1"].
                    getService(Components.interfaces.nsICrashReporter);
                  return true;
                } catch (exc) {
                  return false;
                }
            """)

    def check_for_crashes(self, dump_directory, *args, **kwargs):
        minidump_files = glob.glob('{}/*.dmp'.format(dump_directory))
        shutil.rmtree(dump_directory, ignore_errors=True)

        if self.crash_reporter_enabled:
            return len(minidump_files)
        else:
            return len(minidump_files) == 0

    def log_crashes(self, logger, dump_directory, *args, **kwargs):
        return self.check_for_crashes(dump_directory, *args, **kwargs)


class BaseCrashTestCase(MarionetteTestCase):

    # Reduce the timeout for faster processing of the tests
    socket_timeout = 10

    def setUp(self):
        super(BaseCrashTestCase, self).setUp()

        self.mozcrash_mock = MockMozCrash(self.marionette)
        self.crash_count = self.marionette.crashed
        self.pid = self.marionette.process_id
        self.remote_uri = self.marionette.absolute_url("javascriptPage.html")

    def tearDown(self):
        self.marionette.crashed = self.crash_count

        super(BaseCrashTestCase, self).tearDown()

    def crash(self, chrome=True):
        context = 'chrome' if chrome else 'content'
        sandbox = None if chrome else 'system'

        # Monkey patch mozcrash to avoid crash info output only for our triggered crashes.
        mozcrash = runner.mozcrash
        runner.mozcrash = self.mozcrash_mock

        socket_timeout = self.marionette.client.socket_timeout

        self.marionette.set_context(context)
        try:
            self.marionette.client.socket_timeout = self.socket_timeout
            self.marionette.execute_script("""
              // Copied from crash me simple
              Components.utils.import("resource://gre/modules/ctypes.jsm");

              // ctypes checks for NULL pointer derefs, so just go near-NULL.
              var zero = new ctypes.intptr_t(8);
              var badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
              var crash = badptr.contents;
            """, sandbox=sandbox)
        finally:
            runner.mozcrash = mozcrash
            self.marionette.client.socket_timeout = socket_timeout


class TestCrash(BaseCrashTestCase):

    def test_crash_chrome_process(self):
        self.assertRaisesRegexp(IOError, 'Process crashed',
                                self.crash, chrome=True)
        self.assertEqual(self.marionette.crashed, 1)
        self.assertIsNone(self.marionette.session)
        self.assertRaisesRegexp(MarionetteException, 'Please start a session',
                                self.marionette.get_url)

        self.marionette.start_session()
        self.assertNotEqual(self.marionette.process_id, self.pid)

        # TODO: Bug 1314594 - Causes a hang for the communication between the
        # chrome and frame script.
        # self.marionette.get_url()

    @run_if_e10s
    def test_crash_content_process(self):
        # If e10s is disabled the chrome process crashes
        self.marionette.navigate(self.remote_uri)

        self.assertRaisesRegexp(IOError, 'Content process crashed',
                                self.crash, chrome=False)
        self.assertEqual(self.marionette.crashed, 1)
        self.assertIsNone(self.marionette.session)
        self.assertRaisesRegexp(MarionetteException, 'Please start a session',
                                self.marionette.get_url)

        self.marionette.start_session()
        self.assertNotEqual(self.marionette.process_id, self.pid)
        self.marionette.get_url()

    @expectedFailure
    def test_unexpected_crash(self):
        self.crash(chrome=True)


class TestCrashInSetUp(BaseCrashTestCase):

    def setUp(self):
        super(TestCrashInSetUp, self).setUp()

        self.assertRaisesRegexp(IOError, 'Process crashed',
                                self.crash, chrome=True)
        self.assertEqual(self.marionette.crashed, 1)
        self.assertIsNone(self.marionette.session)

    def test_crash_in_setup(self):
        self.marionette.start_session()
        self.assertNotEqual(self.marionette.process_id, self.pid)


class TestCrashInTearDown(BaseCrashTestCase):

    def tearDown(self):
        try:
            self.assertRaisesRegexp(IOError, 'Process crashed',
                                    self.crash, chrome=True)
        finally:
            self.assertEqual(self.marionette.crashed, 1)
            self.assertIsNone(self.marionette.session)
            super(TestCrashInTearDown, self).tearDown()

    def test_crash_in_teardown(self):
        pass
