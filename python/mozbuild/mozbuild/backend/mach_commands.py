# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import subprocess

from mozbuild.base import MachCommandBase
from mozbuild.build_commands import Build

from mozfile import which
from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

import mozpack.path as mozpath


@CommandProvider
class MachCommands(MachCommandBase):
    @Command("ide", category="devenv", description="Generate a project and launch an IDE.")
    @CommandArgument("ide", choices=["eclipse", "visualstudio", "vscode"])
    @CommandArgument("args", nargs=argparse.REMAINDER)
    def eclipse(self, ide, args):
        if ide == "eclipse":
            backend = "CppEclipse"
        elif ide == "visualstudio":
            backend = "VisualStudio"
        elif ide == "vscode":
            backend = "Clangd"

        if ide == "eclipse" and not which("eclipse"):
            self.log(
                logging.ERROR,
                "ide",
                {},
                "Eclipse CDT 8.4 or later must be installed in your PATH.",
            )
            self.log(
                logging.ERROR, "ide", {}, "Download: http://www.eclipse.org/cdt/downloads.php"
            )
            return 1

        if ide == "vscode":
            # Verify if platform has VSCode installed
            if not self.found_vscode_path():
                self.log(logging.ERROR, "ide", {}, "VSCode cannot be found, abording!")
                return 1

            # Create the Build environment to configure the tree
            builder = Build(self._mach_context, None)

            rc = builder.configure()
            if rc != 0:
                return rc

            # First install what we can through install manifests.
            rc = builder._run_make(
                directory=self.topobjdir, target="pre-export", line_handler=None
            )
            if rc != 0:
                return rc

            # Then build the rest of the build dependencies by running the full
            # export target, because we can't do anything better.
            for target in ("export", "pre-compile"):
                rc = builder._run_make(directory=self.topobjdir, target=target, line_handler=None)
                if rc != 0:
                    return rc
        else:
            # Here we refresh the whole build. 'build export' is sufficient here and is
            # probably more correct but it's also nice having a single target to get a fully
            # built and indexed project (gives a easy target to use before go out to lunch).
            res = self._mach_context.commands.dispatch("build", self._mach_context)
            if res != 0:
                return 1

        # Generate or refresh the IDE backend.
        python = self.virtualenv_manager.python_path
        config_status = os.path.join(self.topobjdir, "config.status")
        args = [python, config_status, "--backend=%s" % backend]
        res = self._run_command_in_objdir(args=args, pass_thru=True, ensure_exit_code=False)
        if res != 0:
            return 1

        if ide == "eclipse":
            eclipse_workspace_dir = self.get_eclipse_workspace_path()
            subprocess.check_call(["eclipse", "-data", eclipse_workspace_dir])
        elif ide == "visualstudio":
            visual_studio_workspace_dir = self.get_visualstudio_workspace_path()
            subprocess.check_call(["explorer.exe", visual_studio_workspace_dir])
        elif ide == "vscode":
            return self.setup_vscode()

    def get_eclipse_workspace_path(self):
        from mozbuild.backend.cpp_eclipse import CppEclipseBackend

        return CppEclipseBackend.get_workspace_path(self.topsrcdir, self.topobjdir)

    def get_visualstudio_workspace_path(self):
        return os.path.join(self.topobjdir, "msvc", "mozilla.sln")

    def found_vscode_path(self):

        if "linux" in self.platform[0]:
            self.vscode_path = "/usr/bin/code"
        elif "macos" in self.platform[0]:
            self.vscode_path = "/usr/local/bin/code"
        elif "win64" in self.platform[0]:
            from pathlib import Path

            self.vscode_path = mozpath.join(
                str(Path.home()), "AppData", "Local", "Programs", "Microsoft VS Code", "Code.exe",
            )

        # Path found
        if os.path.exists(self.vscode_path):
            return True

        for _ in range(5):
            self.vscode_path = input(
                "Could not find the VSCode binary. Please provide the full path to it:\n"
            )
            if os.path.exists(self.vscode_path):
                return True

        # Path cannot be found
        return False

    def setup_vscode(self):
        vscode_settings = mozpath.join(self.topsrcdir, ".vscode", "settings.json")

        clangd_cc_path = mozpath.join(self.topobjdir, "clangd")

        # Verify if the required files are present
        clang_tools_path = mozpath.join(self._mach_context.state_dir, "clang-tools")
        clang_tidy_bin = mozpath.join(clang_tools_path, "clang-tidy", "bin")

        clangd_path = mozpath.join(
            clang_tidy_bin, "clangd" + self.config_environment.substs.get("BIN_SUFFIX", ""),
        )

        if not os.path.exists(clangd_path):
            self.log(
                logging.ERROR, "ide", {}, "Unable to locate clangd in {}.".format(clang_tidy_bin)
            )
            rc = self._get_clang_tools(clang_tools_path)

            if rc != 0:
                return rc

        import multiprocessing
        import json
        from mozbuild.code_analysis.utils import ClangTidyConfig

        clang_tidy_cfg = ClangTidyConfig(self.topsrcdir)

        clangd_json = json.loads(
            """
        {
            "clangd.path": "%s",
            "clangd.arguments": [
                "--compile-commands-dir",
                "%s",
                "-j",
                "%s",
                "--limit-results",
                "0",
                "--completion-style",
                "detailed",
                "--background-index",
                "--all-scopes-completion",
                "--log",
                "error",
                "--pch-storage",
                "memory",
                "--clang-tidy",
                "--clang-tidy-checks",
                "%s"
            ]
        }
        """
            % (
                clangd_path,
                clangd_cc_path,
                multiprocessing.cpu_count(),
                clang_tidy_cfg.checks,
            )
        )

        # Create an empty settings dictionary
        settings = {}

        # Modify the .vscode/settings.json configuration file
        if os.path.exists(vscode_settings):
            # If exists prompt for a configuration change
            choice = prompt_bool(
                "Configuration for {settings} must change. "
                "Do you want to proceed?".format(settings=vscode_settings)
            )
            if not choice:
                return 1

            # Read the original vscode settings
            with open(vscode_settings) as fh:
                try:
                    settings = json.load(fh)
                    print(
                        "The following modifications will occur:\nOriginal:\n{orig}\n"
                        "New:\n{new}".format(
                            orig=json.dumps(
                                {
                                    key: settings[key] if key in settings else ""
                                    for key in ["clangd.path", "clangd.arguments"]
                                },
                                indent=4,
                            ),
                            new=json.dumps(clangd_json, indent=4),
                        )
                    )

                except ValueError:
                    # Decoding has failed, work with an empty dict
                    settings = {}

        # Write our own Configuration
        settings["clangd.path"] = clangd_json["clangd.path"]
        settings["clangd.arguments"] = clangd_json["clangd.arguments"]

        with open(vscode_settings, "w") as fh:
            fh.write(json.dumps(settings, indent=4))

        # Open vscode with new configuration
        rc = subprocess.call([self.vscode_path, self.topsrcdir])

        if rc != 0:
            self.log(
                logging.ERROR,
                "ide",
                {},
                "Unable to open VS Code. Please open VS Code manually and load "
                "directory: {}".format(self.topsrcdir),
            )
            return rc

        return 0

    def _get_clang_tools(self, clang_tools_path):

        import shutil

        if os.path.isdir(clang_tools_path):
            shutil.rmtree(clang_tools_path)

        # Create base directory where we store clang binary
        os.mkdir(clang_tools_path)

        from mozbuild.artifact_commands import PackageFrontend

        self._artifact_manager = PackageFrontend(self._mach_context)

        job, _ = self.platform

        if job is None:
            self.log(
                logging.ERROR,
                "ide",
                {},
                "The current platform isn't supported. "
                "Currently only the following platforms are "
                "supported: win32/win64, linux64 and macosx64.",
            )
            return 1

        job += "-clang-tidy"

        # We want to unpack data in the clang-tidy mozbuild folder
        currentWorkingDir = os.getcwd()
        os.chdir(clang_tools_path)
        rc = self._artifact_manager.artifact_toolchain(
            verbose=False, from_build=[job], no_unpack=False, retry=0
        )
        # Change back the cwd
        os.chdir(currentWorkingDir)

        return rc


def prompt_bool(prompt, limit=5):
    """ Prompts the user with prompt and requires a boolean value. """
    from distutils.util import strtobool

    for _ in range(limit):
        try:
            return strtobool(input(prompt + " [Y/N]\n"))
        except ValueError:
            print(
                "ERROR! Please enter a valid option! Please use any of the following:"
                " Y, N, True, False, 1, 0"
            )
    return False
