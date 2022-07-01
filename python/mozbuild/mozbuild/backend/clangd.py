# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This module provides a backend for `clangd` in order to have support for
# code completion, compile errors, go-to-definition and more.
# It is based on `database.py` with the difference that we don't generate
# an unified `compile_commands.json` but we generate a per file basis `command` in
# `objdir/clangd/compile_commands.json`

from __future__ import absolute_import, print_function

import os

from mozbuild.compilation.database import CompileDBBackend

import mozpack.path as mozpath


class ClangdBackend(CompileDBBackend):
    """
    Configuration that generates the backend for clangd, it is used with `clangd`
    extension for vscode
    """

    def _init(self):
        CompileDBBackend._init(self)

    def _get_compiler_args(self, cenv, canonical_suffix):
        compiler_args = super(ClangdBackend, self)._get_compiler_args(
            cenv, canonical_suffix
        )
        if compiler_args is None:
            return None

        if compiler_args[0][-6:] == "ccache":
            compiler_args.pop(0)
        return compiler_args

    def _build_cmd(self, cmd, filename, unified):
        cmd = list(cmd)

        cmd.append(filename)

        return cmd

    def _outputfile_path(self):
        clangd_cc_path = os.path.join(self.environment.topobjdir, "clangd")

        if not os.path.exists(clangd_cc_path):
            os.mkdir(clangd_cc_path)

        # Output the database (a JSON file) to objdir/clangd/compile_commands.json
        return mozpath.join(clangd_cc_path, "compile_commands.json")

    def _process_unified_sources(self, obj):
        for f in list(sorted(obj.files)):
            self._build_db_line(
                obj.objdir, obj.relsrcdir, obj.config, f, obj.canonical_suffix
            )
