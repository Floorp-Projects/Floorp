# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import logging
import os
import subprocess

from mozbuild import build_commands

from mozfile import which
from mach.decorators import CommandArgument, Command

import mozpack.path as mozpath


@Command(
    "ide",
    category="devenv",
    description="Generate a project and launch an IDE.",
    virtualenv_name="build",
)
@CommandArgument("ide", choices=["eclipse", "visualstudio", "vscode"])
@CommandArgument("args", nargs=argparse.REMAINDER)
def run(command_context, ide, args):
    if ide == "eclipse":
        backend = "CppEclipse"
    elif ide == "visualstudio":
        backend = "VisualStudio"
    elif ide == "vscode":
        backend = "Clangd"

    if ide == "eclipse" and not which("eclipse"):
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Eclipse CDT 8.4 or later must be installed in your PATH.",
        )
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Download: http://www.eclipse.org/cdt/downloads.php",
        )
        return 1

    if ide == "vscode":
        from .clangd import find_vscode_cmd

        # Check if platform has VSCode installed
        vscode_cmd = find_vscode_cmd()
        if vscode_cmd is None:
            choice = prompt_bool(
                "VSCode cannot be found, and may not be installed. Proceed?"
            )
            if not choice:
                return 1

        rc = build_commands.configure(command_context)

        if rc != 0:
            return rc

        # First install what we can through install manifests.
        rc = command_context._run_make(
            directory=command_context.topobjdir,
            target="pre-export",
            line_handler=None,
        )
        if rc != 0:
            return rc

        # Then build the rest of the build dependencies by running the full
        # export target, because we can't do anything better.
        for target in ("export", "pre-compile"):
            rc = command_context._run_make(
                directory=command_context.topobjdir,
                target=target,
                line_handler=None,
            )
            if rc != 0:
                return rc
    else:
        # Here we refresh the whole build. 'build export' is sufficient here and is
        # probably more correct but it's also nice having a single target to get a fully
        # built and indexed project (gives a easy target to use before go out to lunch).
        res = command_context._mach_context.commands.dispatch(
            "build", command_context._mach_context
        )
        if res != 0:
            return 1

    # Generate or refresh the IDE backend.
    python = command_context.virtualenv_manager.python_path
    config_status = os.path.join(command_context.topobjdir, "config.status")
    args = [python, config_status, "--backend=%s" % backend]
    res = command_context._run_command_in_objdir(
        args=args, pass_thru=True, ensure_exit_code=False
    )
    if res != 0:
        return 1

    if ide == "eclipse":
        eclipse_workspace_dir = get_eclipse_workspace_path(command_context)
        subprocess.check_call(["eclipse", "-data", eclipse_workspace_dir])
    elif ide == "visualstudio":
        visual_studio_workspace_dir = get_visualstudio_workspace_path(command_context)
        subprocess.call(["explorer.exe", visual_studio_workspace_dir])
    elif ide == "vscode":
        return setup_vscode(command_context, vscode_cmd)


def get_eclipse_workspace_path(command_context):
    from mozbuild.backend.cpp_eclipse import CppEclipseBackend

    return CppEclipseBackend.get_workspace_path(
        command_context.topsrcdir, command_context.topobjdir
    )


def get_visualstudio_workspace_path(command_context):
    return os.path.normpath(
        os.path.join(command_context.topobjdir, "msvc", "mozilla.sln")
    )


def setup_vscode(command_context, vscode_cmd):
    vscode_settings = mozpath.join(
        command_context.topsrcdir, ".vscode", "settings.json"
    )

    clangd_cc_path = mozpath.join(command_context.topobjdir, "clangd")

    # Verify if the required files are present
    clang_tools_path = mozpath.join(
        command_context._mach_context.state_dir, "clang-tools"
    )
    clang_tidy_bin = mozpath.join(clang_tools_path, "clang-tidy", "bin")

    clangd_path = mozpath.join(
        clang_tidy_bin,
        "clangd" + command_context.config_environment.substs.get("BIN_SUFFIX", ""),
    )

    if not os.path.exists(clangd_path):
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Unable to locate clangd in {}.".format(clang_tidy_bin),
        )
        rc = get_clang_tools(command_context, clang_tools_path)

        if rc != 0:
            return rc

    import multiprocessing
    import json
    import difflib
    from mozbuild.code_analysis.utils import ClangTidyConfig

    clang_tidy_cfg = ClangTidyConfig(command_context.topsrcdir)

    clangd_json = {
        "clangd.path": clangd_path,
        "clangd.arguments": [
            "--compile-commands-dir",
            clangd_cc_path,
            "-j",
            str(multiprocessing.cpu_count() // 2),
            "--limit-results",
            "0",
            "--completion-style",
            "detailed",
            "--background-index",
            "--all-scopes-completion",
            "--log",
            "info",
            "--pch-storage",
            "memory",
            "--clang-tidy",
        ],
    }

    clang_tidy = {}
    clang_tidy["Checks"] = ",".join(clang_tidy_cfg.checks)
    clang_tidy.update(clang_tidy_cfg.checks_config)

    # Write .clang-tidy yml
    import yaml

    with open(".clang-tidy", "w") as file:
        yaml.dump(clang_tidy, file)

    # Load the existing .vscode/settings.json file, to check if if needs to
    # be created or updated.
    try:
        with open(vscode_settings) as fh:
            old_settings_str = fh.read()
    except FileNotFoundError:
        print("Configuration for {} will be created.".format(vscode_settings))
        old_settings_str = None

    if old_settings_str is None:
        # No old settings exist
        with open(vscode_settings, "w") as fh:
            json.dump(clangd_json, fh, indent=4)
    else:
        # Merge our new settings with the existing settings, and check if we
        # need to make changes. Only prompt & write out the updated config
        # file if settings actually changed.
        try:
            old_settings = json.loads(old_settings_str)
            prompt_prefix = ""
        except ValueError:
            old_settings = {}
            prompt_prefix = (
                "\n**WARNING**: Parsing of existing settings file failed. "
                "Existing settings will be lost!"
            )

        settings = {**old_settings, **clangd_json}

        if old_settings != settings:
            # Prompt the user with a diff of the changes we're going to make
            new_settings_str = json.dumps(settings, indent=4)
            print(
                "\nThe following modifications to {settings} will occur:\n{diff}".format(
                    settings=vscode_settings,
                    diff="".join(
                        difflib.unified_diff(
                            old_settings_str.splitlines(keepends=True),
                            new_settings_str.splitlines(keepends=True),
                            "a/.vscode/settings.json",
                            "b/.vscode/settings.json",
                            n=30,
                        )
                    ),
                )
            )
            choice = prompt_bool(
                "{}\nProceed with modifications to {}?".format(
                    prompt_prefix, vscode_settings
                )
            )
            if not choice:
                return 1

            with open(vscode_settings, "w") as fh:
                fh.write(new_settings_str)

    # Open vscode with new configuration, or ask the user to do so if the
    # binary was not found.
    if vscode_cmd is None:
        print(
            "Please open VS Code manually and load directory: {}".format(
                command_context.topsrcdir
            )
        )
        return 0

    rc = subprocess.call(vscode_cmd + [command_context.topsrcdir])

    if rc != 0:
        command_context.log(
            logging.ERROR,
            "ide",
            {},
            "Unable to open VS Code. Please open VS Code manually and load "
            "directory: {}".format(command_context.topsrcdir),
        )
        return rc

    return 0


def get_clang_tools(command_context, clang_tools_path):

    import shutil

    if os.path.isdir(clang_tools_path):
        shutil.rmtree(clang_tools_path)

    # Create base directory where we store clang binary
    os.mkdir(clang_tools_path)

    from mozbuild.artifact_commands import artifact_toolchain

    job, _ = command_context.platform

    if job is None:
        command_context.log(
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
    rc = artifact_toolchain(
        command_context, verbose=False, from_build=[job], no_unpack=False, retry=0
    )
    # Change back the cwd
    os.chdir(currentWorkingDir)

    return rc


def prompt_bool(prompt, limit=5):
    """Prompts the user with prompt and requires a boolean value."""
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
