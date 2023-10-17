# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
import signal
import subprocess
from collections import namedtuple

from mozboot.util import get_tools_dir
from mozfile import which
from mozlint import result
from mozlint.pathutils import expand_exclusions
from packaging.version import Version

RUSTFMT_NOT_FOUND = """
Could not find rustfmt! Install rustfmt and try again.

    $ rustup component add rustfmt

And make sure that it is in the PATH
""".strip()


RUSTFMT_INSTALL_ERROR = """
Unable to install correct version of rustfmt
Try to install it manually with:
    $ rustup component add rustfmt
""".strip()


RUSTFMT_WRONG_VERSION = """
You are probably using an old version of rustfmt.
Expected version is {version}.
Try to update it:
    $ rustup update stable
""".strip()


def parse_issues(config, output, paths):
    RustfmtDiff = namedtuple("RustfmtDiff", ["file", "line", "diff"])
    issues = []
    diff_line = re.compile("^Diff in (.*) at line ([0-9]*):")
    file = ""
    line_no = 0
    diff = ""
    for line in output.split(b"\n"):
        processed_line = (
            line.decode("utf-8", "replace") if isinstance(line, bytes) else line
        ).rstrip("\r\n")
        match = diff_line.match(processed_line)
        if match:
            if diff:
                issues.append(RustfmtDiff(file, line_no, diff.rstrip("\n")))
                diff = ""
            file, line_no = match.groups()
        else:
            diff += processed_line + "\n"
    # the algorithm above will always skip adding the last issue
    issues.append(RustfmtDiff(file, line_no, diff))
    file = os.path.normcase(os.path.normpath(file))
    results = []
    for issue in issues:
        # rustfmt can not be supplied the paths to the files we want to analyze
        # therefore, for each issue detected, we check if any of the the paths
        # supplied are part of the file name.
        # This just filters out the issues that are not part of paths.
        if any([os.path.normcase(os.path.normpath(path)) in file for path in paths]):
            res = {
                "path": issue.file,
                "diff": issue.diff,
                "level": "warning",
                "lineno": issue.line,
            }
            results.append(result.from_config(config, **res))
    return {"results": results, "fixed": 0}


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


def get_rustfmt_binary():
    """
    Returns the path of the first rustfmt binary available
    if not found returns None
    """
    binary = os.environ.get("RUSTFMT")
    if binary:
        return binary

    rust_path = os.path.join(get_tools_dir(), "rustc", "bin")
    return which("rustfmt", path=os.pathsep.join([rust_path, os.environ["PATH"]]))


def get_rustfmt_version(binary):
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

    version = re.findall(r"\d.\d+.\d+", output)[0]
    return Version(version)


def lint(paths, config, fix=None, **lintargs):
    log = lintargs["log"]
    paths = list(expand_exclusions(paths, config, lintargs["root"]))

    # An empty path array can occur when the user passes in `-n`. If we don't
    # return early in this case, rustfmt will attempt to read stdin and hang.
    if not paths:
        return []

    binary = get_rustfmt_binary()

    if not binary:
        print(RUSTFMT_NOT_FOUND)
        if "MOZ_AUTOMATION" in os.environ:
            return 1
        return []

    min_version_str = config.get("min_rustfmt_version")
    min_version = Version(min_version_str)
    actual_version = get_rustfmt_version(binary)
    log.debug(
        "Found version: {}. Minimal expected version: {}".format(
            actual_version, min_version
        )
    )

    if actual_version < min_version:
        print(RUSTFMT_WRONG_VERSION.format(version=min_version_str))
        return 1

    cmd_args = [binary]
    cmd_args.append("--check")
    base_command = cmd_args + paths
    log.debug("Command: {}".format(" ".join(cmd_args)))
    output = run_process(config, base_command)

    issues = parse_issues(config, output, paths)

    if fix:
        issues["fixed"] = len(issues["results"])
        issues["results"] = []
        cmd_args.remove("--check")

        base_command = cmd_args + paths
        log.debug("Command: {}".format(" ".join(cmd_args)))
        output = run_process(config, base_command)

    return issues
