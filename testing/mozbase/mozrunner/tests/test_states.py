#!/usr/bin/env python

import os
from time import sleep
import unittest

import mozrunner

import mozrunnertest


class MozrunnerStatesTestCase(mozrunnertest.MozrunnerTestCase):

    def test_errors_before_start(self):
        """Bug 965714: Not started errors before start() is called"""

        def test_returncode():
            return self.runner.returncode

        self.assertRaises(mozrunner.RunnerNotStartedError, self.runner.is_running)
        self.assertRaises(mozrunner.RunnerNotStartedError, test_returncode)
        self.assertRaises(mozrunner.RunnerNotStartedError, self.runner.wait)
