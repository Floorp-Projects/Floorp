# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, unicode_literals, print_function
import os
import unittest
from unittest import mock

from mozunit import main
from mach.registrar import Registrar
from mozbuild.base import MozbuildObject
import mozpack.path as mozpath


class TestStaticAnalysis(unittest.TestCase):
    def setUp(self):
        self.remove_cats = []
        for cat in ("build", "post-build", "misc", "testing"):
            if cat in Registrar.categories:
                continue
            Registrar.register_category(cat, cat, cat)
            self.remove_cats.append(cat)

    def tearDown(self):
        for cat in self.remove_cats:
            del Registrar.categories[cat]
            del Registrar.commands_by_category[cat]

    def test_bug_1615884(self):
        # TODO: cleaner test
        # we're testing the `_is_ignored_path` but in an ideal
        # world we should test the clang_analysis mach command
        # since that small function is an internal detail.
        # But there is zero test infra for that mach command
        from mozbuild.code_analysis.mach_commands import StaticAnalysis

        config = MozbuildObject.from_environment()
        context = mock.MagicMock()
        context.cwd = config.topsrcdir

        cmd = StaticAnalysis(context)
        cmd.topsrcdir = os.path.join("/root", "dir")
        path = os.path.join("/root", "dir", "path1")

        ignored_dirs_re = r"path1|path2/here|path3\there"
        self.assertTrue(cmd._is_ignored_path(ignored_dirs_re, path) is not None)

        # simulating a win32 env
        win32_path = "\\root\\dir\\path1"
        cmd.topsrcdir = "\\root\\dir"
        old_sep = os.sep
        os.sep = "\\"
        try:
            self.assertTrue(
                cmd._is_ignored_path(ignored_dirs_re, win32_path) is not None
            )
        finally:
            os.sep = old_sep

        self.assertTrue(cmd._is_ignored_path(ignored_dirs_re, "path2") is None)

    def test_get_files(self):
        from mozbuild.code_analysis.mach_commands import StaticAnalysis

        config = MozbuildObject.from_environment()
        context = mock.MagicMock()
        context.cwd = config.topsrcdir

        cmd = StaticAnalysis(context)
        cmd.topsrcdir = mozpath.join("/root", "dir")
        source = cmd.get_abspath_files(["file1", mozpath.join("directory", "file2")])

        self.assertTrue(
            source
            == [
                mozpath.join(cmd.topsrcdir, "file1"),
                mozpath.join(cmd.topsrcdir, "directory", "file2"),
            ]
        )


if __name__ == "__main__":
    main()
