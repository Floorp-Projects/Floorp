# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Defines a set of constants shared by test runners and other scripts."""

import os


CHROME_PACKAGE = 'com.google.android.apps.chrome'
CHROME_ACTIVITY = 'com.google.android.apps.chrome.Main'
CHROME_TESTS_PACKAGE = 'com.google.android.apps.chrome.tests'
LEGACY_BROWSER_PACKAGE = 'com.google.android.browser'
LEGACY_BROWSER_ACTIVITY = 'com.android.browser.BrowserActivity'
CONTENT_SHELL_PACKAGE = "org.chromium.content_shell"
CONTENT_SHELL_ACTIVITY = "org.chromium.content_shell.ContentShellActivity"
CHROME_SHELL_PACKAGE = 'org.chromium.chrome.browser.test'
CHROMIUM_TEST_SHELL_PACKAGE = 'org.chromium.chrome.testshell'

CHROME_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__),
                                          '..', '..', '..'))

# Ports arrangement for various test servers used in Chrome for Android.
# Lighttpd server will attempt to use 9000 as default port, if unavailable it
# will find a free port from 8001 - 8999.
LIGHTTPD_DEFAULT_PORT = 9000
LIGHTTPD_RANDOM_PORT_FIRST = 8001
LIGHTTPD_RANDOM_PORT_LAST = 8999
TEST_SYNC_SERVER_PORT = 9031

# The net test server is started from 10000. Reserve 20000 ports for the all
# test-server based tests should be enough for allocating different port for
# individual test-server based test.
TEST_SERVER_PORT_FIRST = 10000
TEST_SERVER_PORT_LAST = 30000
# A file to record next valid port of test server.
TEST_SERVER_PORT_FILE = '/tmp/test_server_port'
TEST_SERVER_PORT_LOCKFILE = '/tmp/test_server_port.lock'

TEST_EXECUTABLE_DIR = '/data/local/tmp'
# Directories for common java libraries for SDK build.
# These constants are defined in build/android/ant/common.xml
SDK_BUILD_TEST_JAVALIB_DIR = 'test.lib.java'
SDK_BUILD_APKS_DIR = 'apks'

# The directory on the device where perf test output gets saved to.
DEVICE_PERF_OUTPUT_DIR = '/data/data/' + CHROME_PACKAGE + '/files'
