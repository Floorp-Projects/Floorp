# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
import os
import unittest
from unittest import mock

import mozpack.path as mozpath
from mach.registrar import Registrar
from mozunit import main

from mozbuild.base import MozbuildObject


class TestStaticAnalysis(unittest.TestCase):
    def setUp(self):
        self.remove_cats = []
        for cat in ("build", "post-build", "misc", "testing", "devenv"):
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
        from mozbuild.code_analysis.mach_commands import _is_ignored_path

        config = MozbuildObject.from_environment()
        context = mock.MagicMock()
        context.cwd = config.topsrcdir

        command_context = mock.MagicMock()
        command_context.topsrcdir = os.path.join("/root", "dir")
        path = os.path.join("/root", "dir", "path1")

        ignored_dirs_re = r"path1|path2/here|path3\there"
        self.assertTrue(
            _is_ignored_path(command_context, ignored_dirs_re, path) is not None
        )

        # simulating a win32 env
        win32_path = "\\root\\dir\\path1"
        command_context.topsrcdir = "\\root\\dir"
        old_sep = os.sep
        os.sep = "\\"
        try:
            self.assertTrue(
                _is_ignored_path(command_context, ignored_dirs_re, win32_path)
                is not None
            )
        finally:
            os.sep = old_sep

        self.assertTrue(
            _is_ignored_path(command_context, ignored_dirs_re, "path2") is None
        )

    def test_get_files(self):
        from mozbuild.code_analysis.mach_commands import get_abspath_files

        config = MozbuildObject.from_environment()
        context = mock.MagicMock()
        context.cwd = config.topsrcdir

        command_context = mock.MagicMock()
        command_context.topsrcdir = mozpath.join("/root", "dir")
        source = get_abspath_files(
            command_context, ["file1", mozpath.join("directory", "file2")]
        )

        self.assertTrue(
            source
            == [
                mozpath.join(command_context.topsrcdir, "file1"),
                mozpath.join(command_context.topsrcdir, "directory", "file2"),
            ]
        )


if __name__ == "__main__":
    main()
