# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import signal
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "eslint"))
from eslint import setup_helper

from mozbuild.nodeutil import find_node_executable

from mozlint import result

ESLINT_ERROR_MESSAGE = """
An error occurred running eslint. Please check the following error messages:

{}
""".strip()

ESLINT_NOT_FOUND_MESSAGE = """
Could not find eslint!  We looked at the --binary option, at the ESLINT
environment variable, and then at your local node_modules path. Please Install
eslint and needed plugins with:

mach eslint --setup

and try again.
""".strip()


def setup(root, **lintargs):
    setup_helper.set_project_root(root)

    if not setup_helper.check_node_executables_valid():
        return 1

    return setup_helper.eslint_maybe_setup()


def lint(paths, config, binary=None, fix=None, rules=[], setup=None, **lintargs):
    """Run eslint."""
    log = lintargs["log"]
    setup_helper.set_project_root(lintargs["root"])
    module_path = setup_helper.get_project_root()

    # Valid binaries are:
    #  - Any provided by the binary argument.
    #  - Any pointed at by the ESLINT environmental variable.
    #  - Those provided by |mach lint --setup|.

    if not binary:
        binary, _ = find_node_executable()

    if not binary:
        print(ESLINT_NOT_FOUND_MESSAGE)
        return 1

    extra_args = lintargs.get("extra_args") or []
    exclude_args = []
    for path in config.get("exclude", []):
        exclude_args.extend(
            ["--ignore-pattern", os.path.relpath(path, lintargs["root"])]
        )

    for rule in rules:
        extra_args.extend(["--rule", rule])

    cmd_args = (
        [
            binary,
            os.path.join(module_path, "node_modules", "eslint", "bin", "eslint.js"),
            # This keeps ext as a single argument.
            "--ext",
            "[{}]".format(",".join(config["extensions"])),
            "--format",
            "json",
            "--no-error-on-unmatched-pattern",
        ]
        + rules
        + extra_args
        + exclude_args
        + paths
    )
    log.debug("Command: {}".format(" ".join(cmd_args)))
    results = run(cmd_args, config)
    fixed = 0
    # eslint requires that --fix be set before the --ext argument.
    if fix:
        fixed += len(results)
        cmd_args.insert(2, "--fix")
        results = run(cmd_args, config)
        fixed = fixed - len(results)

    return {"results": results, "fixed": fixed}


def run(cmd_args, config):

    shell = False
    if (
        os.environ.get("MSYSTEM") in ("MINGW32", "MINGW64")
        or "MOZILLABUILD" in os.environ
    ):
        # The eslint binary needs to be run from a shell with msys
        shell = True
    encoding = "utf-8"

    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = subprocess.Popen(
        cmd_args, shell=shell, stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    signal.signal(signal.SIGINT, orig)

    try:
        output, errors = proc.communicate()
    except KeyboardInterrupt:
        proc.kill()
        return []

    if errors:
        errors = errors.decode(encoding, "replace")
        print(ESLINT_ERROR_MESSAGE.format(errors))

    if proc.returncode >= 2:
        return 1

    if not output:
        return []  # no output means success
    output = output.decode(encoding, "replace")
    try:
        jsonresult = json.loads(output)
    except ValueError:
        print(ESLINT_ERROR_MESSAGE.format(output))
        return 1

    results = []
    for obj in jsonresult:
        errors = obj["messages"]

        for err in errors:
            err.update(
                {
                    "hint": err.get("fix"),
                    "level": "error" if err["severity"] == 2 else "warning",
                    "lineno": err.get("line") or 0,
                    "path": obj["filePath"],
                    "rule": err.get("ruleId"),
                }
            )
            results.append(result.from_config(config, **err))

    return results
