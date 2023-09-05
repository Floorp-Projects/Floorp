# -*- Mode: python; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*-
# vim: set filetype=python:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import re
import signal
import subprocess
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), "eslint"))
from eslint import setup_helper
from mozbuild.nodeutil import find_node_executable
from mozlint import result

STYLELINT_ERROR_MESSAGE = """
An error occurred running stylelint. Please check the following error messages:

{}
""".strip()

STYLELINT_NOT_FOUND_MESSAGE = """
Could not find stylelint!  We looked at the --binary option, at the STYLELINT
environment variable, and then at your local node_modules path. Please install
eslint, stylelint and needed plugins with:

mach eslint --setup

and try again.
""".strip()

FILE_EXT_REGEX = re.compile(r"\.[a-z0-9_]{2,10}$", re.IGNORECASE)


def setup(root, **lintargs):
    setup_helper.set_project_root(root)

    if not setup_helper.check_node_executables_valid():
        return 1

    return setup_helper.eslint_maybe_setup()


def lint(paths, config, binary=None, fix=None, rules=[], setup=None, **lintargs):
    """Run stylelint."""
    log = lintargs["log"]
    setup_helper.set_project_root(lintargs["root"])
    module_path = setup_helper.get_project_root()

    modified_paths = []
    exts = "*.{" + ",".join(config["extensions"]) + "}"

    for path in paths:
        filepath, fileext = os.path.splitext(path)
        if fileext:
            modified_paths += [path]
        else:
            joined_path = os.path.join(path, "**", exts)
            if is_windows():
                joined_path = joined_path.replace("\\", "/")
            modified_paths.append(joined_path)

    # Valid binaries are:
    #  - Any provided by the binary argument.
    #  - Any pointed at by the STYLELINT environmental variable.
    #  - Those provided by |mach lint --setup|.

    if not binary:
        binary, _ = find_node_executable()

    if not binary:
        print(STYLELINT_NOT_FOUND_MESSAGE)
        return 1

    extra_args = lintargs.get("extra_args") or []
    exclude_args = []
    for path in config.get("exclude", []):
        exclude_args.extend(
            ["--ignore-pattern", os.path.relpath(path, lintargs["root"])]
        )

    # Default to $topsrcdir/.stylelintrc.js, but allow override in stylelint.yml
    stylelint_rc = config.get("stylelint-rc", ".stylelintrc.js")

    # First run Stylelint
    cmd_args = (
        [
            binary,
            os.path.join(
                module_path, "node_modules", "stylelint", "bin", "stylelint.mjs"
            ),
            "--formatter",
            "json",
            "--allow-empty-input",
            "--config",
            os.path.join(lintargs["root"], stylelint_rc),
        ]
        + extra_args
        + exclude_args
        + modified_paths
    )

    if fix:
        cmd_args.append("--fix")

    log.debug("Stylelint command: {}".format(" ".join(cmd_args)))

    result = run(cmd_args, config, fix)
    if result == 1:
        return result

    return result


def run(cmd_args, config, fix):
    shell = False
    if is_windows():
        # The stylelint binary needs to be run from a shell with msys
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
        print(STYLELINT_ERROR_MESSAGE.format(errors))

    # 0 is success, 2 is there was at least 1 rule violation. Anything else
    # is more serious.
    if proc.returncode != 0 and proc.returncode != 2:
        if proc.returncode == 78:
            print("Stylelint reported an issue with its configuration file.")
            print(output)
        return 1

    if not output:
        return {"results": [], "fixed": 0}  # no output means success
    output = output.decode(encoding, "replace")
    try:
        jsonresult = json.loads(output)
    except ValueError:
        print(STYLELINT_ERROR_MESSAGE.format(output))
        return 1

    results = []
    fixed = 0
    for obj in jsonresult:
        errors = obj["warnings"] + obj["parseErrors"]
        # This will return a number of fixed files, as that's the only thing
        # stylelint gives us. Note that it also seems to sometimes list files
        # like this where it finds nothing and fixes nothing. It's not clear
        # why... but this is why we also check if we were even trying to fix
        # anything.
        if fix and not errors and not obj.get("ignored"):
            fixed += 1

        for err in errors:
            msg = err.get("text")
            if err.get("rule"):
                # stylelint includes the rule id in the error message.
                # All mozlint formatters that include the error message also already
                # separately include the rule id, so that leads to duplication. Fix:
                msg = msg.replace("(" + err.get("rule") + ")", "").strip()
            err.update(
                {
                    "message": msg,
                    "level": err.get("severity") or "error",
                    "lineno": err.get("line") or 0,
                    "path": obj["source"],
                    "rule": err.get("rule") or "parseError",
                }
            )
            results.append(result.from_config(config, **err))

    return {"results": results, "fixed": fixed}


def is_windows():
    return (
        os.environ.get("MSYSTEM") in ("MINGW32", "MINGW64")
        or "MOZILLABUILD" in os.environ
    )
