#!/usr/bin/env python
# ***** BEGIN LICENSE BLOCK *****
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.
# ***** END LICENSE BLOCK *****
"""Mozilla error lists for running tests.

Error lists are used to parse output in mozharness.base.log.OutputParser.

Each line of output is matched against each substring or regular expression
in the error list.  On a match, we determine the 'level' of that line,
whether IGNORE, DEBUG, INFO, WARNING, ERROR, CRITICAL, or FATAL.

"""

import re
from mozharness.base.log import INFO, WARNING, ERROR

# ErrorLists {{{1
_mochitest_summary = {
    'regex': re.compile(r'''(\d+ INFO (Passed|Failed|Todo):\ +(\d+)|\t(Passed|Failed|Todo): (\d+))'''),
    'pass_group': "Passed",
    'fail_group': "Failed",
    'known_fail_group': "Todo",
}

TinderBoxPrintRe = {
    "mochitest_summary": _mochitest_summary,
    "mochitest-chrome_summary": _mochitest_summary,
    "mochitest-gl_summary": _mochitest_summary,
    "mochitest-media_summary": _mochitest_summary,
    "mochitest-plain-clipboard_summary": _mochitest_summary,
    "mochitest-plain-gpu_summary": _mochitest_summary,
    "marionette_summary": {
        'regex': re.compile(r'''(passed|failed|todo):\ +(\d+)'''),
        'pass_group': "passed",
        'fail_group': "failed",
        'known_fail_group': "todo",
    },
    "reftest_summary": {
        'regex': re.compile(r'''REFTEST INFO \| (Successful|Unexpected|Known problems): (\d+) \('''),
        'pass_group': "Successful",
        'fail_group': "Unexpected",
        'known_fail_group': "Known problems",
    },
    "crashtest_summary": {
        'regex': re.compile(r'''REFTEST INFO \| (Successful|Unexpected|Known problems): (\d+) \('''),
        'pass_group': "Successful",
        'fail_group': "Unexpected",
        'known_fail_group': "Known problems",
    },
    "xpcshell_summary": {
        'regex': re.compile(r'''INFO \| (Passed|Failed): (\d+)'''),
        'pass_group': "Passed",
        'fail_group': "Failed",
        'known_fail_group': None,
    },
    "jsreftest_summary": {
        'regex': re.compile(r'''REFTEST INFO \| (Successful|Unexpected|Known problems): (\d+) \('''),
        'pass_group': "Successful",
        'fail_group': "Unexpected",
        'known_fail_group': "Known problems",
    },
    "robocop_summary": _mochitest_summary,
    "instrumentation_summary": _mochitest_summary,
    "cppunittest_summary": {
        'regex': re.compile(r'''cppunittests INFO \| (Passed|Failed): (\d+)'''),
        'pass_group': "Passed",
        'fail_group': "Failed",
        'known_fail_group': None,
    },
    "gtest_summary": {
        'regex': re.compile(r'''(Passed|Failed): (\d+)'''),
        'pass_group': "Passed",
        'fail_group': "Failed",
        'known_fail_group': None,
    },
    "jittest_summary": {
        'regex': re.compile(r'''(Passed|Failed): (\d+)'''),
        'pass_group': "Passed",
        'fail_group': "Failed",
        'known_fail_group': None,
    },
    "mozbase_summary": {
        'regex': re.compile(r'''(OK)|(FAILED) \(errors=(\d+)'''),
        'pass_group': "OK",
        'fail_group': "FAILED",
        'known_fail_group': None,
    },
    "mozmill_summary": {
        'regex': re.compile(r'''INFO (Passed|Failed|Skipped): (\d+)'''),
        'pass_group': "Passed",
        'fail_group': "Failed",
        'known_fail_group': "Skipped",
    },

    "harness_error": {
        'full_regex': re.compile(r"(?:TEST-UNEXPECTED-FAIL|PROCESS-CRASH) \| .* \| (application crashed|missing output line for total leaks!|negative leaks caught!|\d+ bytes leaked)"),
        'minimum_regex': re.compile(r'''(TEST-UNEXPECTED|PROCESS-CRASH)'''),
        'retry_regex': re.compile(r'''(FAIL-SHOULD-RETRY|No space left on device|DMError|Connection to the other side was lost in a non-clean fashion|program finished with exit code 80|INFRA-ERROR|twisted.spread.pb.PBConnectionLost|_dl_open: Assertion)''')
    },
}

TestPassed = [
    {'regex': re.compile('''(TEST-INFO|TEST-KNOWN-FAIL|TEST-PASS|INFO \| )'''), 'level': INFO},
]

HarnessErrorList = [
    {'substr': 'TEST-UNEXPECTED', 'level': ERROR, },
    {'substr': 'PROCESS-CRASH', 'level': ERROR, },
]

LogcatErrorList = [
    {'substr': 'Fatal signal 11 (SIGSEGV)', 'level': ERROR, 'explanation': 'This usually indicates the B2G process has crashed'},
    {'substr': 'Fatal signal 7 (SIGBUS)', 'level': ERROR, 'explanation': 'This usually indicates the B2G process has crashed'},
    {'substr': '[JavaScript Error:', 'level': WARNING},
    {'substr': 'seccomp sandbox violation', 'level': ERROR, 'explanation': 'A content process has violated the system call sandbox (bug 790923)'},
]
