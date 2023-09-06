# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import signal
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "eslint"))
from mozbuild.nodeutil import find_node_executable
from mozlint import result

from eslint import setup_helper

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

PRETTIER_ERROR_MESSAGE = """
An error occurred running prettier. Please check the following error messages:

{}
""".strip()

PRETTIER_FORMATTING_MESSAGE = (
    "This file needs formatting with Prettier (use 'mach lint --fix <path>')."
)


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

    # First run ESLint
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

    if fix:
        # eslint requires that --fix be set before the --ext argument.
        cmd_args.insert(2, "--fix")

    log.debug("ESLint command: {}".format(" ".join(cmd_args)))

    result = run(cmd_args, config)
    if result == 1:
        return result

    # Then run Prettier
    cmd_args = (
        [
            binary,
            os.path.join(module_path, "node_modules", "prettier", "bin-prettier.js"),
            "--list-different",
            "--no-error-on-unmatched-pattern",
        ]
        + extra_args
        # Prettier does not support exclude arguments.
        # + exclude_args
        + paths
    )
    log.debug("Prettier command: {}".format(" ".join(cmd_args)))

    if fix:
        cmd_args.append("--write")

    prettier_result = run_prettier(cmd_args, config, fix)
    if prettier_result == 1:
        return prettier_result

    result["results"].extend(prettier_result["results"])
    result["fixed"] = result["fixed"] + prettier_result["fixed"]
    return result


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
        return {"results": [], "fixed": 0}

    if errors:
        errors = errors.decode(encoding, "replace")
        print(ESLINT_ERROR_MESSAGE.format(errors))

    if proc.returncode >= 2:
        return 1

    if not output:
        return {"results": [], "fixed": 0}  # no output means success
    output = output.decode(encoding, "replace")
    try:
        jsonresult = json.loads(output)
    except ValueError:
        print(ESLINT_ERROR_MESSAGE.format(output))
        return 1

    results = []
    fixed = 0
    for obj in jsonresult:
        errors = obj["messages"]
        # This will return a count of files fixed, rather than issues fixed, as
        # that is the only count we have.
        if "output" in obj:
            fixed = fixed + 1

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

    return {"results": results, "fixed": fixed}


def run_prettier(cmd_args, config, fix):
    shell = False
    if is_windows():
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
        return {"results": [], "fixed": 0}

    results = []

    if errors:
        errors = errors.decode(encoding, "replace").strip().split("\n")
        errors = [
            error
            for error in errors
            # Unknown options are not an issue for Prettier, this avoids
            # errors during tests.
            if not ("Ignored unknown option" in error)
        ]
        if len(errors):
            results.append(
                result.from_config(
                    config,
                    **{
                        "name": "eslint",
                        "path": os.path.abspath("."),
                        "message": PRETTIER_ERROR_MESSAGE.format("\n".join(errors)),
                        "level": "error",
                        "rule": "prettier",
                        "lineno": 0,
                        "column": 0,
                    }
                )
            )

    if not output:
        # If we have errors, but no output, we assume something really bad happened.
        if errors and len(errors):
            return {"results": results, "fixed": 0}

        return {"results": [], "fixed": 0}  # no output means success

    output = output.decode(encoding, "replace").splitlines()

    fixed = 0

    if fix:
        # When Prettier is running in fix mode, it outputs the list of files
        # that have been fixed, so sum them up here.
        # If it can't fix files, it will throw an error, which will be handled
        # above.
        fixed = len(output)
    else:
        # When in "check" mode, Prettier will output the list of files that
        # need changing, so we'll wrap them in our result structure here.
        for file in output:
            if not file:
                continue

            file = os.path.abspath(file)
            results.append(
                result.from_config(
                    config,
                    **{
                        "name": "eslint",
                        "path": file,
                        "message": PRETTIER_FORMATTING_MESSAGE,
                        "level": "error",
                        "rule": "prettier",
                        "lineno": 0,
                        "column": 0,
                    }
                )
            )

    return {"results": results, "fixed": fixed}


def is_windows():
    return (
        os.environ.get("MSYSTEM") in ("MINGW32", "MINGW64")
        or "MOZILLABUILD" in os.environ
    )
