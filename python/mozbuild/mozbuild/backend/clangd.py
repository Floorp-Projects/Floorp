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


def find_vscode_cmd():
    import shutil
    import sys

    # Try to look up the `code` binary on $PATH, and use it if present. This
    # should catch cases like being run from within a vscode-remote shell,
    # even if vscode itself is also installed on the remote host.
    path = shutil.which("code")
    if path is not None:
        return [path]

    cmd_and_path = []

    # If the binary wasn't on $PATH, try to find it in a variety of other
    # well-known install locations based on the current platform.
    if sys.platform.startswith("darwin"):
        cmd_and_path = [
            {"path": "/usr/local/bin/code", "cmd": ["/usr/local/bin/code"]},
            {
                "path": "/Applications/Visual Studio Code.app",
                "cmd": ["open", "/Applications/Visual Studio Code.app", "--args"],
            },
            {
                "path": "/Applications/Visual Studio Code - Insiders.app",
                "cmd": [
                    "open",
                    "/Applications/Visual Studio Code - Insiders.app",
                    "--args",
                ],
            },
        ]
    elif sys.platform.startswith("win"):
        from pathlib import Path

        vscode_path = mozpath.join(
            str(Path.home()),
            "AppData",
            "Local",
            "Programs",
            "Microsoft VS Code",
            "Code.exe",
        )
        vscode_insiders_path = mozpath.join(
            str(Path.home()),
            "AppData",
            "Local",
            "Programs",
            "Microsoft VS Code Insiders",
            "Code - Insiders.exe",
        )
        cmd_and_path = [
            {"path": vscode_path, "cmd": [vscode_path]},
            {"path": vscode_insiders_path, "cmd": [vscode_insiders_path]},
        ]
    elif sys.platform.startswith("linux"):
        cmd_and_path = [
            {"path": "/usr/local/bin/code", "cmd": ["/usr/local/bin/code"]},
            {"path": "/snap/bin/code", "cmd": ["/snap/bin/code"]},
            {"path": "/usr/bin/code", "cmd": ["/usr/bin/code"]},
            {"path": "/usr/bin/code-insiders", "cmd": ["/usr/bin/code-insiders"]},
        ]

    # Did we guess the path?
    for element in cmd_and_path:
        if os.path.exists(element["path"]):
            return element["cmd"]

    # Path cannot be found
    return None


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
