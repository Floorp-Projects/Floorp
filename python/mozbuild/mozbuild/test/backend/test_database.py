# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import os
import six

from mozbuild.compilation.database import CompileDBBackend
from mozbuild.backend.clangd import ClangdBackend
from mozbuild.backend.static_analysis import StaticAnalysisBackend

from mozbuild.test.backend.common import BackendTester

from mozunit import main


class TestCompileDBBackends(BackendTester):
    def perform_check(self, compile_commands_path, topsrcdir, topobjdir):
        self.assertTrue(os.path.exists(compile_commands_path))
        compile_db = json.loads(open(compile_commands_path, "r").read())

        # Verify that we have the same number of items
        self.assertEqual(len(compile_db), 4)

        expected_db = [
            {
                "directory": topobjdir,
                "command": "clang -o /dev/null -c -ferror-limit=0 {}/bar.c".format(
                    topsrcdir
                ),
                "file": "{}/bar.c".format(topsrcdir),
            },
            {
                "directory": topobjdir,
                "command": "clang -o /dev/null -c -ferror-limit=0 {}/foo.c".format(
                    topsrcdir
                ),
                "file": "{}/foo.c".format(topsrcdir),
            },
            {
                "directory": topobjdir,
                "command": "clang++ -o /dev/null -c -ferror-limit=0 {}/bar.cpp".format(
                    topsrcdir
                ),
                "file": "{}/bar.cpp".format(topsrcdir),
            },
            {
                "directory": topobjdir,
                "command": "clang++ -o /dev/null -c -ferror-limit=0 {}/foo.cpp".format(
                    topsrcdir
                ),
                "file": "{}/foo.cpp".format(topsrcdir),
            },
        ]

        # Verify item consistency against `expected_db`
        six.assertCountEqual(self, compile_db, expected_db)

    def test_database(self):
        """Ensure we can generate a `compile_commands.json` and that is correct."""

        env = self._consume("database", CompileDBBackend)
        compile_commands_path = os.path.join(env.topobjdir, "compile_commands.json")

        self.perform_check(compile_commands_path, env.topsrcdir, env.topobjdir)

    def test_clangd(self):
        """Ensure we can generate a `compile_commands.json` and that is correct.
        in order to be used by ClandBackend"""

        env = self._consume("database", ClangdBackend)
        compile_commands_path = os.path.join(
            env.topobjdir, "clangd", "compile_commands.json"
        )

        self.perform_check(compile_commands_path, env.topsrcdir, env.topobjdir)

    def test_static_analysis(self):
        """Ensure we can generate a `compile_commands.json` and that is correct.
        in order to be used by StaticAnalysisBackend"""

        env = self._consume("database", StaticAnalysisBackend)
        compile_commands_path = os.path.join(
            env.topobjdir, "static-analysis", "compile_commands.json"
        )

        self.perform_check(compile_commands_path, env.topsrcdir, env.topobjdir)


if __name__ == "__main__":
    main()
