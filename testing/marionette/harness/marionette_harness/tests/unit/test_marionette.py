# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import time

from marionette_driver import errors
from marionette_harness import MarionetteTestCase, run_if_manage_instance, skip_if_mobile


class TestMarionette(MarionetteTestCase):

    def test_correct_test_name(self):
        """Test that the correct test name gets set."""
        expected_test_name = '{module}.py {cls}.{func}'.format(
            module=__name__,
            cls=self.__class__.__name__,
            func=self.test_correct_test_name.__name__,
        )

        self.assertEqual(self.marionette.test_name, expected_test_name)

    @run_if_manage_instance("Only runnable if Marionette manages the instance")
    @skip_if_mobile("Bug 1322993 - Missing temporary folder")
    def test_wait_for_port_non_existing_process(self):
        """Test that wait_for_port doesn't run into a timeout if instance is not running."""
        self.marionette.quit()
        self.assertIsNotNone(self.marionette.instance.runner.returncode)
        start_time = time.time()
        self.assertFalse(self.marionette.wait_for_port(timeout=5))
        self.assertLess(time.time() - start_time, 5)


class TestContext(MarionetteTestCase):

    def setUp(self):
        MarionetteTestCase.setUp(self)
        self.marionette.set_context(self.marionette.CONTEXT_CONTENT)

    def get_context(self):
        return self.marionette._send_message("getContext", key="value")

    def set_context(self, value):
        return self.marionette._send_message("setContext", {"value": value})

    def test_set_context(self):
        self.assertEqual(self.set_context("content"), {})
        self.assertEqual(self.set_context("chrome"), {})

        for typ in [True, 42, [], {}, None]:
            with self.assertRaises(errors.InvalidArgumentException):
                self.set_context(typ)

        with self.assertRaises(errors.MarionetteException):
            self.set_context("foo")

    def test_get_context(self):
        self.assertEqual(self.get_context(), "content")
        self.set_context("chrome")
        self.assertEqual(self.get_context(), "chrome")
        self.set_context("content")
        self.assertEqual(self.get_context(), "content")
