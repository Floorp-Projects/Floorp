# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Constants for SCHEDULES configuration in moz.build files and for
skip-unless-schedules optimizations in task-graph generation.
"""

# TODO: ideally these lists could be specified in moz.build itself

# Inclusive components are those which are scheduled when certain files are
# changed, but do not run by default.  These are generally added to
# `SCHEDULES.inclusive` using `+=`, but can also be used as exclusive
# components for files which *only* affect the named component.
INCLUSIVE_COMPONENTS = [
    "docs",
    "py-lint",
    "js-lint",
    "yaml-lint",
    # inclusive test suites -- these *only* run when certain files have changed
    "jittest",
    "test-verify",
    "test-verify-gpu",
    "test-verify-wpt",
    "test-coverage",
    "test-coverage-wpt",
    "jsreftest",
    "android-hw-gfx",
    "rusttests",
]
INCLUSIVE_COMPONENTS = sorted(INCLUSIVE_COMPONENTS)

# Exclusive components are those which are scheduled by default, but for which
# some files *only* affect that component.  For example, most files affect all
# platforms, but platform-specific files exclusively affect a single platform.
# These components are assigned to `SCHEDULES.exclusive` with `=`. Each comment
# denotes a new mutually exclusive set of groups that tasks can belong to.
EXCLUSIVE_COMPONENTS = [
    # os families
    "android",
    "linux",
    "macosx",
    "windows",
    # broad test harness categories
    "awsy",
    "condprofile",
    "cppunittest",
    "firefox-ui",
    "fuzztest",
    "geckoview-junit",
    "gtest",
    "marionette",
    "mochitest",
    "raptor",
    "reftest",
    "talos",
    "telemetry-tests-client",
    "xpcshell",
    "xpcshell-coverage",
    "web-platform-tests",
    # specific test suites
    "crashtest",
    "mochitest-a11y",
    "mochitest-browser-a11y",
    "mochitest-browser-media",
    "mochitest-browser-chrome",
    "mochitest-chrome",
    "mochitest-plain",
    "web-platform-tests-crashtest",
    "web-platform-tests-print-reftest",
    "web-platform-tests-reftest",
    "web-platform-tests-wdspec",
    "nss",
]
EXCLUSIVE_COMPONENTS = sorted(EXCLUSIVE_COMPONENTS)
ALL_COMPONENTS = INCLUSIVE_COMPONENTS + EXCLUSIVE_COMPONENTS
