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
import unittest

import mozpack.path as mozpath
import mozunit
from mozbuild.base import MozbuildObject
from mozfile import NamedTemporaryFile

from moztest.resolve import (
    TestMetadata,
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


class Base(unittest.TestCase):
    def setUp(self):
        self._temp_files = []

    def tearDown(self):
        for f in self._temp_files:
            del f

        self._temp_files = []

    def _get_test_metadata(self):
        all_tests = NamedTemporaryFile(mode='wb')
        pickle.dump(ALL_TESTS, all_tests)
        all_tests.flush()
        self._temp_files.append(all_tests)

        test_defaults = NamedTemporaryFile(mode='wb')
        pickle.dump(TEST_DEFAULTS, test_defaults)
        test_defaults.flush()
        self._temp_files.append(test_defaults)

        rv = TestMetadata(all_tests.name, "/firefox/", test_defaults=test_defaults.name)
        rv._wpt_loaded = True  # Don't try to load the wpt manifest
        return rv


class TestTestMetadata(Base):
    def test_load(self):
        t = self._get_test_metadata()
        self.assertEqual(len(t._tests_by_path), 8)

        self.assertEqual(len(list(t.tests_with_flavor('xpcshell'))), 3)
        self.assertEqual(len(list(t.tests_with_flavor('mochitest-plain'))), 0)

    def test_resolve_all(self):
        t = self._get_test_metadata()
        self.assertEqual(len(list(t.resolve_tests())), 9)

    def test_resolve_filter_flavor(self):
        t = self._get_test_metadata()
        self.assertEqual(len(list(t.resolve_tests(flavor='xpcshell'))), 4)

    def test_resolve_by_dir(self):
        t = self._get_test_metadata()
        self.assertEqual(len(list(t.resolve_tests(paths=['services/common']))), 2)

    def test_resolve_under_path(self):
        t = self._get_test_metadata()
        self.assertEqual(len(list(t.resolve_tests(under_path='services'))), 2)

        self.assertEqual(len(list(t.resolve_tests(flavor='xpcshell',
            under_path='services'))), 2)

    def test_resolve_multiple_paths(self):
        t = self._get_test_metadata()
        result = list(t.resolve_tests(paths=['services', 'toolkit']))
        self.assertEqual(len(result), 4)

    def test_resolve_support_files(self):
        expected_support_files = "\ndata/**\nxpcshell_updater.ini"
        t = self._get_test_metadata()
        result = list(t.resolve_tests(paths=['toolkit']))
        self.assertEqual(len(result), 2)

        for test in result:
            self.assertEqual(test['support-files'],
                             expected_support_files)

    def test_resolve_path_prefix(self):
        t = self._get_test_metadata()
        result = list(t.resolve_tests(paths=['image']))
        self.assertEqual(len(result), 1)


class TestTestResolver(Base):
    FAKE_TOPSRCDIR = '/firefox'

    def setUp(self):
        Base.setUp(self)

        self._temp_dirs = []

    def tearDown(self):
        Base.tearDown(self)

        for d in self._temp_dirs:
            shutil.rmtree(d)

    def _get_resolver(self):
        topobjdir = tempfile.mkdtemp()
        self._temp_dirs.append(topobjdir)

        with open(os.path.join(topobjdir, 'all-tests.pkl'), 'wb') as fh:
            pickle.dump(ALL_TESTS, fh)
        with open(os.path.join(topobjdir, 'test-defaults.pkl'), 'wb') as fh:
            pickle.dump(TEST_DEFAULTS, fh)

        o = MozbuildObject(self.FAKE_TOPSRCDIR, None, None, topobjdir=topobjdir)

        # Monkey patch the test resolver to avoid tests failing to find make
        # due to our fake topscrdir.
        TestResolver._run_make = lambda *a, **b: None

        return o._spawn(TestResolver)

    def test_cwd_children_only(self):
        """If cwd is defined, only resolve tests under the specified cwd."""
        r = self._get_resolver()

        # Pretend we're under '/services' and ask for 'common'. This should
        # pick up all tests from '/services/common'
        tests = list(r.resolve_tests(paths=['common'], cwd=os.path.join(r.topsrcdir,
            'services')))

        self.assertEqual(len(tests), 2)

        # Tests should be rewritten to objdir.
        for t in tests:
            self.assertEqual(t['here'], mozpath.join(r.topobjdir,
                '_tests/xpcshell/services/common/tests/unit'))

    def test_various_cwd(self):
        """Test various cwd conditions are all equal."""

        r = self._get_resolver()

        expected = list(r.resolve_tests(paths=['services']))
        actual = list(r.resolve_tests(paths=['services'], cwd='/'))
        self.assertEqual(actual, expected)

        actual = list(r.resolve_tests(paths=['services'], cwd=r.topsrcdir))
        self.assertEqual(actual, expected)

        actual = list(r.resolve_tests(paths=['services'], cwd=r.topobjdir))
        self.assertEqual(actual, expected)

    def test_subsuites(self):
        """Test filtering by subsuite."""

        r = self._get_resolver()

        tests = list(r.resolve_tests(paths=['mobile']))
        self.assertEqual(len(tests), 2)

        tests = list(r.resolve_tests(paths=['mobile'], subsuite='browser'))
        self.assertEqual(len(tests), 1)
        self.assertEqual(tests[0]['name'], 'src/TestDistribution.java')

        tests = list(r.resolve_tests(paths=['mobile'], subsuite='background'))
        self.assertEqual(len(tests), 1)
        self.assertEqual(tests[0]['name'], 'src/common/TestAndroidLogWriters.java')

    def test_wildcard_patterns(self):
        """Test matching paths by wildcard."""

        r = self._get_resolver()

        tests = list(r.resolve_tests(paths=['mobile/**']))
        self.assertEqual(len(tests), 2)
        for t in tests:
            self.assertTrue(t['file_relpath'].startswith('mobile'))

        tests = list(r.resolve_tests(paths=['**/**.js', 'accessible/**']))
        self.assertEqual(len(tests), 7)
        for t in tests:
            path = t['file_relpath']
            self.assertTrue(path.startswith('accessible') or path.endswith('.js'))

    def test_resolve_metadata(self):
        """Test finding metadata from outgoing files."""
        r = self._get_resolver()

        suites, tests = r.resolve_metadata(['bc'])
        assert suites == {'mochitest-browser-chrome'}
        assert tests == []

        suites, tests = r.resolve_metadata(['mochitest-a11y', '/browser', 'xpcshell'])
        assert suites == {'mochitest-a11y', 'xpcshell'}
        assert sorted(t['file_relpath'] for t in tests) == [
            'devtools/client/markupview/test/browser_markupview_copy_image_data.js',
            'image/test/browser/browser_bug666317.js',
            'mobile/android/tests/browser/junit3/src/TestDistribution.java',
        ]

    def test_task_regexes(self):
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
