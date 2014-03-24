# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import shutil
import tempfile
import unittest

import mozpack.path as mozpath

from mozfile import NamedTemporaryFile
from mozunit import main

from mozbuild.base import MozbuildObject
from mozbuild.testing import (
    TestMetadata,
    TestResolver,
)


ALL_TESTS_JSON = b'''
{
    "accessible/tests/mochitest/actions/test_anchors.html": [
        {
            "dir_relpath": "accessible/tests/mochitest/actions",
            "expected": "pass",
            "file_relpath": "accessible/tests/mochitest/actions/test_anchors.html",
            "flavor": "a11y",
            "here": "/Users/gps/src/firefox/accessible/tests/mochitest/actions",
            "manifest": "/Users/gps/src/firefox/accessible/tests/mochitest/actions/a11y.ini",
            "name": "test_anchors.html",
            "path": "/Users/gps/src/firefox/accessible/tests/mochitest/actions/test_anchors.html",
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
            "here": "/Users/gps/src/firefox/services/common/tests/unit",
            "manifest": "/Users/gps/src/firefox/services/common/tests/unit/xpcshell.ini",
            "name": "test_async_chain.js",
            "path": "/Users/gps/src/firefox/services/common/tests/unit/test_async_chain.js",
            "relpath": "test_async_chain.js",
            "tail": ""
        }
    ],
    "services/common/tests/unit/test_async_querySpinningly.js": [
        {
            "dir_relpath": "services/common/tests/unit",
            "file_relpath": "services/common/tests/unit/test_async_querySpinningly.js",
            "firefox-appdir": "browser",
            "flavor": "xpcshell",
            "head": "head_global.js head_helpers.js head_http.js",
            "here": "/Users/gps/src/firefox/services/common/tests/unit",
            "manifest": "/Users/gps/src/firefox/services/common/tests/unit/xpcshell.ini",
            "name": "test_async_querySpinningly.js",
            "path": "/Users/gps/src/firefox/services/common/tests/unit/test_async_querySpinningly.js",
            "relpath": "test_async_querySpinningly.js",
            "tail": ""
        }
    ],
   "toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js": [
        {
            "dir_relpath": "toolkit/mozapps/update/test/unit",
            "file_relpath": "toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "flavor": "xpcshell",
            "generated-files": "head_update.js",
            "head": "head_update.js",
            "here": "/Users/gps/src/firefox/toolkit/mozapps/update/test/unit",
            "manifest": "/Users/gps/src/firefox/toolkit/mozapps/update/test/unit/xpcshell_updater.ini",
            "name": "test_0201_app_launch_apply_update.js",
            "path": "/Users/gps/src/firefox/toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "reason": "bug 820380",
            "relpath": "test_0201_app_launch_apply_update.js",
            "run-sequentially": "Launches application.",
            "skip-if": "toolkit == 'gonk' || os == 'android'",
            "support-files": "\\ndata/**\\nxpcshell_updater.ini",
            "tail": ""
        },
        {
            "dir_relpath": "toolkit/mozapps/update/test/unit",
            "file_relpath": "toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "flavor": "xpcshell",
            "generated-files": "head_update.js",
            "head": "head_update.js head2.js",
            "here": "/Users/gps/src/firefox/toolkit/mozapps/update/test/unit",
            "manifest": "/Users/gps/src/firefox/toolkit/mozapps/update/test/unit/xpcshell_updater.ini",
            "name": "test_0201_app_launch_apply_update.js",
            "path": "/Users/gps/src/firefox/toolkit/mozapps/update/test/unit/test_0201_app_launch_apply_update.js",
            "reason": "bug 820380",
            "relpath": "test_0201_app_launch_apply_update.js",
            "run-sequentially": "Launches application.",
            "skip-if": "toolkit == 'gonk' || os == 'android'",
            "support-files": "\\ndata/**\\nxpcshell_updater.ini",
            "tail": ""
        }
    ]
}'''.strip()


class Base(unittest.TestCase):
    def setUp(self):
        self._temp_files = []

    def tearDown(self):
        for f in self._temp_files:
            del f

        self._temp_files = []

    def _get_test_metadata(self):
        f = NamedTemporaryFile()
        f.write(ALL_TESTS_JSON)
        f.flush()
        self._temp_files.append(f)

        return TestMetadata(filename=f.name)


class TestTestMetadata(Base):
    def test_load(self):
        t = self._get_test_metadata()
        self.assertEqual(len(t._tests_by_path), 4)

        self.assertEqual(len(list(t.tests_with_flavor('xpcshell'))), 3)
        self.assertEqual(len(list(t.tests_with_flavor('mochitest-plain'))), 0)

    def test_resolve_all(self):
        t = self._get_test_metadata()
        self.assertEqual(len(list(t.resolve_tests())), 5)

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


class TestTestResolver(Base):
    FAKE_TOPSRCDIR = '/Users/gps/src/firefox'

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

        with open(os.path.join(topobjdir, 'all-tests.json'), 'wt') as fh:
            fh.write(ALL_TESTS_JSON)

        o = MozbuildObject(self.FAKE_TOPSRCDIR, None, None, topobjdir=topobjdir)

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


if __name__ == '__main__':
    main()
