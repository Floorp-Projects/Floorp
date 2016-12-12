# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

__version__ = '4.0.0'

from .marionette_test import (
    CommonTestCase,
    expectedFailure,
    MarionetteTestCase,
    run_if_e10s,
    skip,
    skip_if_chrome,
    skip_if_desktop,
    skip_if_e10s,
    skip_if_mobile,
    SkipTest,
    skip_unless_protocol,
)
from .runner import (
    BaseMarionetteArguments,
    BaseMarionetteTestRunner,
    BrowserMobProxyArguments,
    BrowserMobProxyTestCaseMixin,
    Marionette,
    MarionetteTest,
    MarionetteTestResult,
    MarionetteTextTestRunner,
    TestManifest,
    TestResult,
    TestResultCollection,
    WindowManagerMixin,
)
