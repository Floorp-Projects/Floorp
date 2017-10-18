# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import itertools
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
