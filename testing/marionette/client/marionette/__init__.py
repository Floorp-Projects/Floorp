# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


__version__ = '1.0.0'

from .marionette_test import MarionetteTestCase, MarionetteJSTestCase, CommonTestCase, expectedFailure, skip, SkipTest
from .runner import (
        B2GTestCaseMixin,
        B2GTestResultMixin,
        BaseMarionetteArguments,
        BaseMarionetteTestRunner,
        BrowserMobProxyTestCaseMixin,
        EnduranceArguments,
        EnduranceTestCaseMixin,
        HTMLReportingArguments,
        HTMLReportingTestResultMixin,
        HTMLReportingTestRunnerMixin,
        Marionette,
        MarionetteTest,
        MarionetteTestResult,
        MarionetteTextTestRunner,
        MemoryEnduranceTestCaseMixin,
        TestManifest,
        TestResult,
        TestResultCollection
)
