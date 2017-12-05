# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import shutil

from marionette_driver import Wait
from marionette_driver.errors import (
    MarionetteException,
    NoSuchWindowException,
    TimeoutException
)

from marionette_harness import MarionetteTestCase, expectedFailure, run_if_e10s

# Import runner module to monkey patch mozcrash module
from mozrunner.base import runner


class MockMozCrash(object):
    """Mock object to replace original mozcrash methods."""

    def __init__(self, marionette):
        self.marionette = marionette

        with self.marionette.using_context('chrome'):
            self.crash_reporter_enabled = self.marionette.execute_script("""
                Cu.import("resource://gre/modules/AppConstants.jsm");
                return AppConstants.MOZ_CRASHREPORTER;
            """)

    def check_for_crashes(self, dump_directory, *args, **kwargs):
        if self.crash_reporter_enabled:
            # Workaround until bug 1376795 has been fixed
            # Wait at maximum 5s for the minidump files being created
            # minidump_files = glob.glob('{}/*.dmp'.format(dump_directory))
            try:
                minidump_files = Wait(None, timeout=5).until(
                    lambda _: glob.glob('{}/*.dmp'.format(dump_directory))
                )
            except TimeoutException:
                minidump_files = []

            shutil.rmtree(dump_directory)

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

        # Monkey patch mozcrash to avoid crash info output only for our triggered crashes.
        mozcrash_mock = MockMozCrash(self.marionette)
        if not mozcrash_mock.crash_reporter_enabled:
            self.skipTest('Crash reporter disabled')
            return

        self.mozcrash = runner.mozcrash
        runner.mozcrash = mozcrash_mock

        self.crash_count = self.marionette.crashed
        self.pid = self.marionette.process_id

    def tearDown(self):
        # Replace mockup with original mozcrash instance
        runner.mozcrash = self.mozcrash

        self.marionette.crashed = self.crash_count

        super(BaseCrashTestCase, self).tearDown()

    def crash(self, chrome=True):
        context = 'chrome' if chrome else 'content'
        sandbox = None if chrome else 'system'

        socket_timeout = self.marionette.client.socket_timeout
        self.marionette.client.socket_timeout = self.socket_timeout

        self.marionette.set_context(context)
        try:
            self.marionette.execute_script("""
              // Copied from crash me simple
              Components.utils.import("resource://gre/modules/ctypes.jsm");

              // ctypes checks for NULL pointer derefs, so just go near-NULL.
              var zero = new ctypes.intptr_t(8);
              var badptr = ctypes.cast(zero, ctypes.PointerType(ctypes.int32_t));
              var crash = badptr.contents;
            """, sandbox=sandbox)
        finally:
            self.marionette.client.socket_timeout = socket_timeout


class TestCrash(BaseCrashTestCase):

    def test_crash_chrome_process(self):
        with self.assertRaises(IOError):
            self.crash(chrome=True)
            Wait(self.marionette, timeout=self.socket_timeout,
                 ignored_exceptions=NoSuchWindowException).until(
                lambda _: self.marionette.get_url(),
                message="Expected IOError exception for content crash not raised."
            )

        # A crash results in a non zero exit code
        self.assertNotIn(self.marionette.instance.runner.returncode, (None, 0))

        self.assertEqual(self.marionette.crashed, 1)
        self.assertIsNone(self.marionette.session)
        self.assertRaisesRegexp(MarionetteException, 'Please start a session',
                                self.marionette.get_url)

        self.marionette.start_session()
        self.assertNotEqual(self.marionette.process_id, self.pid)

        self.marionette.get_url()

    @run_if_e10s("Content crashes only exist in e10s mode")
    def test_crash_content_process(self):
        # With MOZ_CRASHREPORTER_SHUTDOWN each window of the browser will be
        # closed in case of a content crash. So the "unload" handler fires for
        # the current window, which causes the command to return early.
        # To check for an IOError, further commands have to be executed until
        # Firefox has been shutdown.
        with self.assertRaises(IOError):
            self.crash(chrome=False)
            Wait(self.marionette, timeout=self.socket_timeout,
                 ignored_exceptions=NoSuchWindowException).until(
                lambda _: self.marionette.get_url(),
                message="Expected IOError exception for content crash not raised."
            )

        # In the case of a content crash Firefox will be closed and its
        # returncode will report 0 (this will change with 1370520).
        self.assertEqual(self.marionette.instance.runner.returncode, 0)

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

        with self.assertRaises(IOError):
            self.crash(chrome=True)
            Wait(self.marionette, timeout=self.socket_timeout,
                 ignored_exceptions=NoSuchWindowException).until(
                lambda _: self.marionette.get_url(),
                message="Expected IOError exception for content crash not raised."
            )

        # A crash results in a non zero exit code
        self.assertNotIn(self.marionette.instance.runner.returncode, (None, 0))

        self.assertEqual(self.marionette.crashed, 1)
        self.assertIsNone(self.marionette.session)

    def test_crash_in_setup(self):
        self.marionette.start_session()
        self.assertNotEqual(self.marionette.process_id, self.pid)


class TestCrashInTearDown(BaseCrashTestCase):

    def tearDown(self):
        try:
            with self.assertRaises(IOError):
                self.crash(chrome=True)
                Wait(self.marionette, timeout=self.socket_timeout,
                     ignored_exceptions=NoSuchWindowException).until(
                    lambda _: self.marionette.get_url(),
                    message="Expected IOError exception for content crash not raised."
                )

            # A crash results in a non zero exit code
            self.assertNotIn(self.marionette.instance.runner.returncode, (None, 0))

            self.assertEqual(self.marionette.crashed, 1)
            self.assertIsNone(self.marionette.session)

        finally:
            super(TestCrashInTearDown, self).tearDown()

    def test_crash_in_teardown(self):
        pass
