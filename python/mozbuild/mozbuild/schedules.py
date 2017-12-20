# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Constants for SCHEDULES configuration in moz.build files and for
skip-unless-schedules optimizations in task-graph generation.
"""

from __future__ import absolute_import, unicode_literals, print_function

# TODO: ideally these lists could be specified in moz.build itself

INCLUSIVE_COMPONENTS = [
    'py-lint',
    'js-lint',
    'yaml-lint',
    # inclusive test suites -- these *only* run when certain files have changed
    'jittest',
    'test-verify',
    'test-verify-wpt',
    'jsreftest',
]
INCLUSIVE_COMPONENTS = sorted(INCLUSIVE_COMPONENTS)

EXCLUSIVE_COMPONENTS = [
    # os families
    'android',
    'linux',
    'macosx',
    'windows',
    # test suites
    'awsy',
    'cppunittest',
    'firefox-ui',
    'geckoview',
    'gtest',
    'marionette',
    'mochitest',
    'reftest',
    'robocop',
    'talos',
    'telemetry-tests-client',
    'xpcshell',
    'xpcshell-coverage',
    'web-platform-tests',
    'web-platform-tests-reftests',
    'web-platform-tests-wdspec',
    # Thunderbird test suites
    'mozmill',
]
EXCLUSIVE_COMPONENTS = sorted(EXCLUSIVE_COMPONENTS)
ALL_COMPONENTS = INCLUSIVE_COMPONENTS + EXCLUSIVE_COMPONENTS
