# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
# flake8: noqa: E501

from __future__ import absolute_import, print_function, unicode_literals

import cPickle as pickle
import os
import re
import shutil
import tempfile

import pytest
import mozpack.path as mozpath
import mozunit
from mozbuild.base import MozbuildObject
from mozfile import NamedTemporaryFile

from moztest.resolve import (
    TestResolver,
    TEST_SUITES,
)


ALL_TESTS = {
    "accessible/tests/mochitest/actions/test_anchors.html": [
        {
            "dir_relpath": "accessible/tests/mochitest/actions",
            "expected": "pass",
            "file_relpath": "accessible/tests/mochitest/actions/test_anchors.html",
            "flavor": "a11y",
            "here": "/firefox/accessible/tests/mochitest/actions",
            "manifest": "/firefox/accessible/tests/mochitest/actions/a11y.ini",
            "name": "test_anchors.html",
            "path": "/firefox/accessible/tests/mochitest/actions/test_anchors.html",
            "relpath": "test_anchors.html"
        }
    ],
    "services/common/tests/unit/test_async_chain.js": [
        {
            "dir_relpath": "services/common/tests/unit",
            "file_relpath": "services/common/tests/unit/test_async_chain.js",
            "firefox-appdir": "browser",
            "flavor": "xpcshell",
            "head": "head_global.js head_helpers.js head_http.js",
            "here": "/firefox/services/common/tests/unit",
            "manifest": "/firefox/services/common/tests/unit/xpcshell.ini",
            "name": "test_async_chain.js",
            "path": "/firefox/services/common/tests/unit/test_async_chain.js",
            "relpath": "test_async_chain.js",
        }
    ],
    "services/common/tests/unit/test_async_querySpinningly.js": [
        {
            "dir_relpath": "services/common/tests/unit",
            "file_relpath": "services/common/tests/unit/test_async_querySpinningly.js",
            "firefox-appdir": "browser",
            "flavor": "xpcshell",
            "head": "head_global.js head_helpers.js head_http.js",
            "here": "/firefox/services/common/tests/unit",
            "manifest": "/firefox/services/common/tests/unit/xpcshell.ini",
            "name": "test_async_querySpinningly.js",
            "path": "/firefox/services/common/tests/unit/test_async_querySpinningly.js",
            "relpath": "test_async_querySpinningly.js",
        }
    ],
    "toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js": [
        {
            "dir_relpath": "toolkit/mozapps/update/test/unit",
            "file_relpath": "toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "flavor": "xpcshell",
            "generated-files": "head_update.js",
            "head": "head_update.js",
            "here": "/firefox/toolkit/mozapps/update/test/unit",
            "manifest": "/firefox/toolkit/mozapps/update/test/unit/xpcshell_updater.ini",
            "name": "test_0201_app_launch_apply_update.js",
            "path": "/firefox/toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "reason": "bug 820380",
            "relpath": "test_0201_app_launch_apply_update.js",
            "run-sequentially": "Launches application.",
            "skip-if": "os == 'android'",
        },
        {
            "dir_relpath": "toolkit/mozapps/update/test/unit",
            "file_relpath": "toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "flavor": "xpcshell",
            "generated-files": "head_update.js",
            "head": "head_update.js head2.js",
            "here": "/firefox/toolkit/mozapps/update/test/unit",
            "manifest": "/firefox/toolkit/mozapps/update/test/unit/xpcshell_updater.ini",
            "name": "test_0201_app_launch_apply_update.js",
            "path": "/firefox/toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "reason": "bug 820380",
            "relpath": "test_0201_app_launch_apply_update.js",
            "run-sequentially": "Launches application.",
            "skip-if": "os == 'android'",
        }
    ],
    "mobile/android/tests/background/junit3/src/common/TestAndroidLogWriters.java": [
        {
            "dir_relpath": "mobile/android/tests/background/junit3/src/common",
            "file_relpath": "mobile/android/tests/background/junit3/src/common/TestAndroidLogWriters.java",
            "flavor": "instrumentation",
            "here": "/firefox/mobile/android/tests/background/junit3",
            "manifest": "/firefox/mobile/android/tests/background/junit3/instrumentation.ini",
            "name": "src/common/TestAndroidLogWriters.java",
            "path": "/firefox/mobile/android/tests/background/junit3/src/common/TestAndroidLogWriters.java",
            "relpath": "src/common/TestAndroidLogWriters.java",
            "subsuite": "background"
        }
    ],
    "mobile/android/tests/browser/junit3/src/TestDistribution.java": [
        {
            "dir_relpath": "mobile/android/tests/browser/junit3/src",
            "file_relpath": "mobile/android/tests/browser/junit3/src/TestDistribution.java",
            "flavor": "instrumentation",
            "here": "/firefox/mobile/android/tests/browser/junit3",
            "manifest": "/firefox/mobile/android/tests/browser/junit3/instrumentation.ini",
            "name": "src/TestDistribution.java",
            "path": "/firefox/mobile/android/tests/browser/junit3/src/TestDistribution.java",
            "relpath": "src/TestDistribution.java",
            "subsuite": "browser"
        }
    ],
    "image/test/browser/browser_bug666317.js": [
        {
            "dir_relpath": "image/test/browser",
            "file_relpath": "image/test/browser/browser_bug666317.js",
            "flavor": "browser-chrome",
            "here": "/firefox/testing/mochitest/browser/image/test/browser",
            "manifest": "/firefox/image/test/browser/browser.ini",
            "name": "browser_bug666317.js",
            "path": "/firefox/testing/mochitest/browser/image/test/browser/browser_bug666317.js",
            "relpath": "image/test/browser/browser_bug666317.js",
            "skip-if": "e10s # Bug 948194 - Decoded Images seem to not be discarded on memory-pressure notification with e10s enabled",
            "subsuite": ""
        }
   ],
   "devtools/client/markupview/test/browser_markupview_copy_image_data.js": [
        {
            "dir_relpath": "devtools/client/markupview/test",
            "file_relpath": "devtools/client/markupview/test/browser_markupview_copy_image_data.js",
            "flavor": "browser-chrome",
            "here": "/firefox/testing/mochitest/browser/devtools/client/markupview/test",
            "manifest": "/firefox/devtools/client/markupview/test/browser.ini",
            "name": "browser_markupview_copy_image_data.js",
            "path": "/firefox/testing/mochitest/browser/devtools/client/markupview/test/browser_markupview_copy_image_data.js",
            "relpath": "devtools/client/markupview/test/browser_markupview_copy_image_data.js",
            "subsuite": "devtools",
            "tags": "devtools"
        }
   ]
}

TEST_DEFAULTS = {
    "/firefox/toolkit/mozapps/update/test/unit/xpcshell_updater.ini": {"support-files": "\ndata/**\nxpcshell_updater.ini"}
}

TASK_LABELS = [
    'test-linux64/opt-browser-screenshots-1',
    'test-linux64/opt-browser-screenshots-e10s-1',
    'test-linux64/opt-marionette',
    'test-linux64/opt-mochitest',
    'test-linux64/debug-mochitest-e10s',
    'test-linux64/opt-mochitest-a11y',
    'test-linux64/opt-mochitest-browser',
    'test-linux64/opt-mochitest-browser-chrome',
    'test-linux64/opt-mochitest-browser-chrome-e10s',
    'test-linux64/opt-mochitest-browser-chrome-e10s-11',
    'test-linux64/opt-mochitest-chrome',
    'test-linux64/opt-mochitest-devtools',
    'test-linux64/opt-mochitest-devtools-chrome',
    'test-linux64/opt-mochitest-gpu',
    'test-linux64/opt-mochitest-gpu-e10s',
    'test-linux64/opt-mochitest-media-e10s-1',
    'test-linux64/opt-mochitest-media-e10s-11',
    'test-linux64/opt-mochitest-plain',
    'test-linux64/opt-mochitest-screenshots-1',
    'test-linux64/opt-reftest',
    'test-linux64/debug-reftest-e10s-1',
    'test-linux64/debug-reftest-e10s-11',
    'test-linux64/opt-robocop',
    'test-linux64/opt-robocop-1',
    'test-linux64/opt-robocop-e10s',
    'test-linux64/opt-robocop-e10s-1',
    'test-linux64/opt-robocop-e10s-11',
    'test-linux64/opt-web-platform-tests-e10s-1',
    'test-linux64/opt-web-platform-tests-reftests-e10s-1',
    'test-linux64/opt-web-platform-tests-reftest-e10s-1',
    'test-linux64/opt-web-platform-tests-wdspec-e10s-1',
    'test-linux64/opt-web-platform-tests-1',
    'test-linux64/opt-web-platform-test-e10s-1',
    'test-linux64/opt-xpcshell',
    'test-linux64/opt-xpcshell-1',
    'test-linux64/opt-xpcshell-2',
]


@pytest.fixture
def resolver(tmpdir):
    topobjdir = tmpdir.mkdir("objdir").strpath

    with open(os.path.join(topobjdir, 'all-tests.pkl'), 'wb') as fh:
        pickle.dump(ALL_TESTS, fh)
    with open(os.path.join(topobjdir, 'test-defaults.pkl'), 'wb') as fh:
        pickle.dump(TEST_DEFAULTS, fh)

    o = MozbuildObject('/firefox', None, None, topobjdir=topobjdir)

    # Monkey patch the test resolver to avoid tests failing to find make
    # due to our fake topscrdir.
    TestResolver._run_make = lambda *a, **b: None
    resolver = o._spawn(TestResolver)
    resolver._puppeteer_loaded = True
    resolver._wpt_loaded = True
    return resolver


def test_load(resolver):
    assert len(resolver.tests_by_path) == 8

    assert len(resolver.tests_by_flavor['xpcshell']) == 3
    assert len(resolver.tests_by_flavor['mochitest-plain']) == 0


def test_resolve_all(resolver):
    assert len(list(resolver._resolve())) == 9


def test_resolve_filter_flavor(resolver):
    assert len(list(resolver._resolve(flavor='xpcshell'))) == 4


def test_resolve_by_dir(resolver):
    assert len(list(resolver._resolve(paths=['services/common']))) == 2


def test_resolve_under_path(resolver):
    assert len(list(resolver._resolve(under_path='services'))) == 2
    assert len(list(resolver._resolve(flavor='xpcshell', under_path='services'))) == 2


def test_resolve_multiple_paths(resolver):
    result = list(resolver.resolve_tests(paths=['services', 'toolkit']))
    assert len(result) == 4


def test_resolve_support_files(resolver):
    expected_support_files = "\ndata/**\nxpcshell_updater.ini"
    result = list(resolver.resolve_tests(paths=['toolkit']))
    assert len(result) == 2

    for test in result:
        assert test['support-files'] == expected_support_files


def test_resolve_path_prefix(resolver):
    result = list(resolver._resolve(paths=['image']))
    assert len(result) == 1


def test_cwd_children_only(resolver):
    """If cwd is defined, only resolve tests under the specified cwd."""
    # Pretend we're under '/services' and ask for 'common'. This should
    # pick up all tests from '/services/common'
    tests = list(resolver.resolve_tests(paths=['common'], cwd=os.path.join(resolver.topsrcdir,
        'services')))

    assert len(tests) == 2

    # Tests should be rewritten to objdir.
    for t in tests:
        assert t['here'] == mozpath.join(resolver.topobjdir,
                                         '_tests/xpcshell/services/common/tests/unit')

def test_various_cwd(resolver):
    """Test various cwd conditions are all equal."""
    expected = list(resolver.resolve_tests(paths=['services']))
    actual = list(resolver.resolve_tests(paths=['services'], cwd='/'))
    assert actual == expected

    actual = list(resolver.resolve_tests(paths=['services'], cwd=resolver.topsrcdir))
    assert actual == expected

    actual = list(resolver.resolve_tests(paths=['services'], cwd=resolver.topobjdir))
    assert actual == expected


def test_subsuites(resolver):
    """Test filtering by subsuite."""
    tests = list(resolver.resolve_tests(paths=['mobile']))
    assert len(tests) == 2

    tests = list(resolver.resolve_tests(paths=['mobile'], subsuite='browser'))
    assert len(tests) == 1
    assert tests[0]['name'] == 'src/TestDistribution.java'

    tests = list(resolver.resolve_tests(paths=['mobile'], subsuite='background'))
    assert len(tests) == 1
    assert tests[0]['name'] == 'src/common/TestAndroidLogWriters.java'


def test_wildcard_patterns(resolver):
    """Test matching paths by wildcard."""
    tests = list(resolver.resolve_tests(paths=['mobile/**']))
    assert len(tests) == 2
    for t in tests:
        assert t['file_relpath'].startswith('mobile')

    tests = list(resolver.resolve_tests(paths=['**/**.js', 'accessible/**']))
    assert len(tests) == 7
    for t in tests:
        path = t['file_relpath']
        assert path.startswith('accessible') or path.endswith('.js')


def test_resolve_metadata(resolver):
    """Test finding metadata from outgoing files."""
    suites, tests = resolver.resolve_metadata(['bc'])
    assert suites == {'mochitest-browser-chrome'}
    assert tests == []

    suites, tests = resolver.resolve_metadata(['mochitest-a11y', '/browser', 'xpcshell'])
    assert suites == {'mochitest-a11y', 'xpcshell'}
    assert sorted(t['file_relpath'] for t in tests) == [
        'devtools/client/markupview/test/browser_markupview_copy_image_data.js',
        'image/test/browser/browser_bug666317.js',
        'mobile/android/tests/browser/junit3/src/TestDistribution.java',
    ]

def test_task_regexes():
    """Test the task_regexes defined in TEST_SUITES."""

    test_cases = {
        'mochitest-browser-chrome': [
            'test-linux64/opt-mochitest-browser-chrome',
            'test-linux64/opt-mochitest-browser-chrome-e10s',
        ],
        'mochitest-chrome': [
            'test-linux64/opt-mochitest-chrome',
        ],
        'mochitest-devtools-chrome': [
            'test-linux64/opt-mochitest-devtools-chrome',
        ],
        'mochitest-media': [
            'test-linux64/opt-mochitest-media-e10s-1',
        ],
        'mochitest-plain': [
            'test-linux64/opt-mochitest',
            'test-linux64/debug-mochitest-e10s',
            # this isn't a real task but the regex would match it if it were
            'test-linux64/opt-mochitest-plain',
        ],
        'mochitest-plain-gpu': [
            'test-linux64/opt-mochitest-gpu',
            'test-linux64/opt-mochitest-gpu-e10s',
        ],
        'mochitest-browser-chrome-screenshots': [
            'test-linux64/opt-browser-screenshots-1',
            'test-linux64/opt-browser-screenshots-e10s-1',
        ],
        'reftest': [
            'test-linux64/opt-reftest',
            'test-linux64/debug-reftest-e10s-1',
        ],
        'robocop': [
            'test-linux64/opt-robocop',
            'test-linux64/opt-robocop-1',
            'test-linux64/opt-robocop-e10s',
            'test-linux64/opt-robocop-e10s-1',
        ],
        'web-platform-tests': [
            'test-linux64/opt-web-platform-tests-e10s-1',
            'test-linux64/opt-web-platform-tests-reftests-e10s-1',
            'test-linux64/opt-web-platform-tests-reftest-e10s-1',
            'test-linux64/opt-web-platform-tests-wdspec-e10s-1',
            'test-linux64/opt-web-platform-tests-1',
        ],
        'web-platform-tests-testharness': [
            'test-linux64/opt-web-platform-tests-e10s-1',
            'test-linux64/opt-web-platform-tests-1',
        ],
        'web-platform-tests-reftest': [
            'test-linux64/opt-web-platform-tests-reftests-e10s-1',
        ],
        'web-platform-tests-wdspec': [
            'test-linux64/opt-web-platform-tests-wdspec-e10s-1',
        ],
        'xpcshell': [
            'test-linux64/opt-xpcshell',
            'test-linux64/opt-xpcshell-1',
        ],
    }

    regexes = []

    def match_task(task):
        return any(re.search(pattern, task) for pattern in regexes)

    for suite, expected in sorted(test_cases.items()):
        print(suite)
        regexes = TEST_SUITES[suite]['task_regex']
        assert set(filter(match_task, TASK_LABELS)) == set(expected)


if __name__ == '__main__':
    mozunit.main()
