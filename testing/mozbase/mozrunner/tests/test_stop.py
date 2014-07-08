#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import signal

import mozrunnertest


class MozrunnerStopTestCase(mozrunnertest.MozrunnerTestCase):

    def test_stop_process(self):
        """Stop the process and test properties"""
        self.runner.start()
        returncode = self.runner.stop()

        self.assertFalse(self.runner.is_running())
        self.assertNotIn(returncode, [None, 0])
        self.assertEqual(self.runner.returncode, returncode)
        self.assertIsNotNone(self.runner.process_handler)

        self.assertEqual(self.runner.wait(1), returncode)

    def test_stop_before_start(self):
        """Stop the process before it gets started should not raise an error"""
        self.runner.stop()

    def test_stop_process_custom_signal(self):
        """Stop the process via a custom signal and test properties"""
        self.runner.start()
        returncode = self.runner.stop(signal.SIGTERM)

        self.assertFalse(self.runner.is_running())
        self.assertNotIn(returncode, [None, 0])
        self.assertEqual(self.runner.returncode, returncode)
        self.assertIsNotNone(self.runner.process_handler)

        self.assertEqual(self.runner.wait(1), returncode)
