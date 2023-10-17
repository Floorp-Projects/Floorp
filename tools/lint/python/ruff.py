# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import platform
import re
import signal
import subprocess
import sys
from pathlib import Path

import mozfile
from mozlint import result

here = os.path.abspath(os.path.dirname(__file__))
RUFF_REQUIREMENTS_PATH = os.path.join(here, "ruff_requirements.txt")

RUFF_NOT_FOUND = """
Could not find ruff! Install ruff and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(
    RUFF_REQUIREMENTS_PATH
)


RUFF_INSTALL_ERROR = """
Unable to install correct version of ruff!
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(
    RUFF_REQUIREMENTS_PATH
)


def default_bindir():
    # We use sys.prefix to find executables as that gets modified with
    # virtualenv's activate_this.py, whereas sys.executable doesn't.
    if platform.system() == "Windows":
        return os.path.join(sys.prefix, "Scripts")
    else:
        return os.path.join(sys.prefix, "bin")


def get_ruff_version(binary):
    """
    Returns found binary's version
    """
    try:
        output = subprocess.check_output(
            [binary, "--version"],
            stderr=subprocess.STDOUT,
            text=True,
        )
    except subprocess.CalledProcessError as e:
        output = e.output

    matches = re.match(r"ruff ([0-9\.]+)", output)
    if matches:
        return matches[1]
    print("Error: Could not parse the version '{}'".format(output))


def setup(root, log, **lintargs):
    virtualenv_bin_path = lintargs.get("virtualenv_bin_path")
    binary = mozfile.which("ruff", path=(virtualenv_bin_path, default_bindir()))

    if binary and os.path.isfile(binary):
        log.debug(f"Looking for ruff at {binary}")
        version = get_ruff_version(binary)
        versions = [
            line.split()[0].strip()
            for line in open(RUFF_REQUIREMENTS_PATH).readlines()
            if line.startswith("ruff==")
        ]
        if [f"ruff=={version}"] == versions:
            log.debug("ruff is present with expected version {}".format(version))
            return 0
        else:
            log.debug("ruff is present but unexpected version {}".format(version))

    virtualenv_manager = lintargs["virtualenv_manager"]
    try:
        virtualenv_manager.install_pip_requirements(RUFF_REQUIREMENTS_PATH, quiet=True)
    except subprocess.CalledProcessError:
        print(RUFF_INSTALL_ERROR)
        return 1


def run_process(config, cmd, **kwargs):
    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True
    )
    signal.signal(signal.SIGINT, orig)
    try:
        output, _ = proc.communicate()
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return output


def lint(paths, config, log, **lintargs):
    fixed = 0
    results = []

    if not paths:
        return {"results": results, "fixed": fixed}

    # Currently ruff only lints non `.py` files if they are explicitly passed
    # in. So we need to find any non-py files manually. This can be removed
    # after https://github.com/charliermarsh/ruff/issues/3410 is fixed.
    exts = [e for e in config["extensions"] if e != "py"]
    non_py_files = []
    for path in paths:
        p = Path(path)
        if not p.is_dir():
            continue
        for ext in exts:
            non_py_files.extend([str(f) for f in p.glob(f"**/*.{ext}")])

    args = ["ruff", "check", "--force-exclude"] + paths + non_py_files

    if config["exclude"]:
        args.append(f"--extend-exclude={','.join(config['exclude'])}")

    process_kwargs = {"processStderrLine": lambda line: log.debug(line)}

    warning_rules = set(config.get("warning-rules", []))
    if lintargs.get("fix"):
        # Do a first pass with --fix-only as the json format doesn't return the
        # number of fixed issues.
        fix_args = args + ["--fix-only"]

        # Don't fix warnings to limit unrelated changes sneaking into patches.
        fix_args.append(f"--extend-ignore={','.join(warning_rules)}")
        output = run_process(config, fix_args, **process_kwargs)
        matches = re.match(r"Fixed (\d+) errors?.", output)
        if matches:
            fixed = int(matches[1])

    log.debug(f"Running with args: {args}")

    output = run_process(config, args + ["--format=json"], **process_kwargs)
    if not output:
        return []

    try:
        issues = json.loads(output)
    except json.JSONDecodeError:
        log.error(f"could not parse output: {output}")
        return []

    for issue in issues:
        res = {
            "path": issue["filename"],
            "lineno": issue["location"]["row"],
            "column": issue["location"]["column"],
            "lineoffset": issue["end_location"]["row"] - issue["location"]["row"],
            "message": issue["message"],
            "rule": issue["code"],
            "level": "error",
        }
        if any(issue["code"].startswith(w) for w in warning_rules):
            res["level"] = "warning"

        if issue["fix"]:
            res["hint"] = issue["fix"]["message"]

        results.append(result.from_config(config, **res))

    return {"results": results, "fixed": fixed}
