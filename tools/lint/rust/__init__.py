# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import signal
import six
import re
import subprocess
from collections import namedtuple

from mozfile import which
from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozprocess import ProcessHandler

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


RUSTFMT_DEPRECATED_VERSION = """
You are probably using the old 'rustfmt' program.
Please use rustfmt-nightly (https://crates.io/crates/rustfmt-nightly)
""".strip()


def parse_issues(config, output, paths):
    RustfmtDiff = namedtuple("RustfmtDiff", ["file", "line", "diff"])
    issues = []
    diff_line = re.compile("^Diff in (.*) at line ([0-9]*):")
    file = ""
    line_no = 0
    diff = ""
    for line in output:
        line = six.ensure_text(line)
        match = diff_line.match(line)
        if match:
            if diff:
                issues.append(RustfmtDiff(file, line_no, diff.rstrip("\n")))
                diff = ""
            file, line_no = match.groups()
        else:
            diff += line + "\n"
    # the algorithm above will always skip adding the last issue
    issues.append(RustfmtDiff(file, line_no, diff))
    results = []
    for issue in issues:
        # rustfmt can not be supplied the paths to the files we want to analyze
        # therefore, for each issue detected, we check if any of the the paths
        # supplied are part of the file name.
        # This just filters out the issues that are not part of paths.
        if any([path in file for path in paths]):
            res = {
                "path": issue.file,
                "diff": issue.diff,
                "level": "warning",
                "lineno": issue.line,
            }
            results.append(result.from_config(config, **res))
    return results


class RustfmtProcess(ProcessHandler):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs["stream"] = False
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(config, cmd):
    proc = RustfmtProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return proc.output


def get_rustfmt_binary():
    """
    Returns the path of the first rustfmt binary available
    if not found returns None
    """
    binary = os.environ.get("RUSTFMT")
    if binary:
        return binary

    return which("rustfmt")


def is_old_rustfmt(binary):
    """
    Check if we are running the deprecated rustfmt
    """
    try:
        output = subprocess.check_output(
            [binary, " --version"],
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
    except subprocess.CalledProcessError as e:
        output = e.output

    if "This version of rustfmt is deprecated" in output:
        return True

    return False


def lint(paths, config, fix=None, **lintargs):
    log = lintargs['log']
    paths = list(expand_exclusions(paths, config, lintargs['root']))

    # An empty path array can occur when the user passes in `-n`. If we don't
    # return early in this case, rustfmt will attempt to read stdin and hang.
    if not paths:
        return []

    binary = get_rustfmt_binary()

    if is_old_rustfmt(binary):
        print(RUSTFMT_DEPRECATED_VERSION)
        return 1

    if not binary:
        print(RUSTFMT_NOT_FOUND)
        if "MOZ_AUTOMATION" in os.environ:
            return 1
        return []

    cmd_args = [binary]
    if not fix:
        cmd_args.append("--check")
    base_command = cmd_args + paths
    log.debug("Command: {}".format(' '.join(cmd_args)))
    output = run_process(config, base_command)

    if fix:
        # Rustfmt is able to fix all issues so don't bother parsing the output.
        return []
    return parse_issues(config, output, paths)
