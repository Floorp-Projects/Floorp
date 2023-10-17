# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform
import re
import signal
import subprocess
import sys

import mozpack.path as mozpath
from mozfile import which
from mozlint import result
from mozlint.pathutils import expand_exclusions

here = os.path.abspath(os.path.dirname(__file__))
BLACK_REQUIREMENTS_PATH = os.path.join(here, "black_requirements.txt")

BLACK_INSTALL_ERROR = """
Unable to install correct version of black
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(
    BLACK_REQUIREMENTS_PATH
)


def default_bindir():
    # We use sys.prefix to find executables as that gets modified with
    # virtualenv's activate_this.py, whereas sys.executable doesn't.
    if platform.system() == "Windows":
        return os.path.join(sys.prefix, "Scripts")
    else:
        return os.path.join(sys.prefix, "bin")


def get_black_version(binary):
    """
    Returns found binary's version
    """
    try:
        output = subprocess.check_output(
            [binary, "--version"],
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
    except subprocess.CalledProcessError as e:
        output = e.output
    try:
        # Accept `black.EXE, version ...` on Windows.
        # for old version of black, the output is
        # black, version 21.4b2
        # From black 21.11b1, the output is like
        # black, 21.11b1 (compiled: no)
        return re.match(r"black.*,( version)? (\S+)", output)[2]
    except TypeError as e:
        print("Could not parse the version '{}'".format(output))
        print("Error: {}".format(e))


def parse_issues(config, output, paths, *, log):
    would_reformat = re.compile("^would reformat (.*)$", re.I)
    reformatted = re.compile("^reformatted (.*)$", re.I)
    cannot_reformat = re.compile("^error: cannot format (.*?): (.*)$", re.I)
    results = []
    for l in output.split(b"\n"):
        line = l.decode("utf-8").rstrip("\r\n")
        if line.startswith("All done!") or line.startswith("Oh no!"):
            break

        match = would_reformat.match(line)
        if match:
            res = {"path": match.group(1), "level": "error"}
            results.append(result.from_config(config, **res))
            continue

        match = reformatted.match(line)
        if match:
            res = {"path": match.group(1), "level": "warning", "message": "reformatted"}
            results.append(result.from_config(config, **res))
            continue

        match = cannot_reformat.match(line)
        if match:
            res = {"path": match.group(1), "level": "error", "message": match.group(2)}
            results.append(result.from_config(config, **res))
            continue

        log.debug(f"Unhandled line: {line}")
    return results


def run_process(config, cmd):
    orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    signal.signal(signal.SIGINT, orig)
    try:
        output, _ = proc.communicate()
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return output


def setup(root, **lintargs):
    log = lintargs["log"]
    virtualenv_bin_path = lintargs.get("virtualenv_bin_path")
    # Using `which` searches multiple directories and handles `.exe` on Windows.
    binary = which("black", path=(virtualenv_bin_path, default_bindir()))

    if binary and os.path.exists(binary):
        binary = mozpath.normsep(binary)
        log.debug("Looking for black at {}".format(binary))
        version = get_black_version(binary)
        versions = [
            line.split()[0].strip()
            for line in open(BLACK_REQUIREMENTS_PATH).readlines()
            if line.startswith("black==")
        ]
        if ["black=={}".format(version)] == versions:
            log.debug("Black is present with expected version {}".format(version))
            return 0
        else:
            log.debug("Black is present but unexpected version {}".format(version))

    log.debug("Black needs to be installed or updated")
    virtualenv_manager = lintargs["virtualenv_manager"]
    try:
        virtualenv_manager.install_pip_requirements(BLACK_REQUIREMENTS_PATH, quiet=True)
    except subprocess.CalledProcessError:
        print(BLACK_INSTALL_ERROR)
        return 1


def run_black(config, paths, fix=None, *, log, virtualenv_bin_path):
    fixed = 0
    binary = os.path.join(virtualenv_bin_path or default_bindir(), "black")

    log.debug("Black version {}".format(get_black_version(binary)))

    cmd_args = [binary]
    if not fix:
        cmd_args.append("--check")

    base_command = cmd_args + paths
    log.debug("Command: {}".format(" ".join(base_command)))
    output = parse_issues(config, run_process(config, base_command), paths, log=log)

    # black returns an issue for fixed files as well
    for eachIssue in output:
        if eachIssue.message == "reformatted":
            fixed += 1

    return {"results": output, "fixed": fixed}


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs["root"]))

    return run_black(
        config,
        files,
        fix=fix,
        log=lintargs["log"],
        virtualenv_bin_path=lintargs.get("virtualenv_bin_path"),
    )
