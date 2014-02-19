#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import mozrunner

import mozrunnertest


class MozrunnerWaitTestCase(mozrunnertest.MozrunnerTestCase):

    def test_wait_while_running(self):
        """Wait for the process while it is running"""
        self.runner.start()
        returncode = self.runner.wait(1)

        self.assertTrue(self.runner.is_running())
        self.assertEqual(returncode, None)
        self.assertEqual(self.runner.returncode, returncode)
        self.assertIsNotNone(self.runner.process_handler)

    def test_wait_after_process_finished(self):
        """Bug 965714: wait() after stop should not raise an error"""
        self.runner.start()
        self.runner.process_handler.kill()

        returncode = self.runner.wait(1)

        self.assertNotIn(returncode, [None, 0])
        self.assertIsNotNone(self.runner.process_handler)
