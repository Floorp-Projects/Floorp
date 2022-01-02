# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

__version__ = "5.0.2"

from .marionette_test import (
    CommonTestCase,
    expectedFailure,
    MarionetteTestCase,
    parameterized,
    run_if_manage_instance,
    skip,
    skip_if_chrome,
    skip_if_desktop,
    SkipTest,
    skip_unless_browser_pref,
    skip_unless_protocol,
    unexpectedSuccess,
)
from .runner import (
    BaseMarionetteArguments,
    BaseMarionetteTestRunner,
    Marionette,
    MarionetteTest,
    MarionetteTestResult,
    MarionetteTextTestRunner,
    TestManifest,
    TestResult,
    TestResultCollection,
    WindowManagerMixin,
)
