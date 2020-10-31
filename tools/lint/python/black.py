# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import platform
import re
import signal
import subprocess
import sys

from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozlint.util import pip
from mozprocess import ProcessHandler

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

    return re.match(r"black, version (.*)$", output)[1]


def parse_issues(config, output, paths, *, log):
    would_reformat = re.compile("^would reformat (.*)$", re.I)
    reformatted = re.compile("^reformatted (.*)$", re.I)
    cannot_reformat = re.compile("^error: cannot format (.*?): (.*)$", re.I)
    results = []
    for line in output:
        line = line.decode("utf-8")
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

        log.debug("Unhandled line", line)
    return results


class BlackProcess(ProcessHandler):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs["stream"] = False
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(config, cmd):
    proc = BlackProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return proc.output


def setup(root, **lintargs):
    if not pip.reinstall_program(BLACK_REQUIREMENTS_PATH):
        print(BLACK_INSTALL_ERROR)
        return 1


def run_black(config, paths, fix=None, *, log, virtualenv_bin_path):
    binary = os.path.join(virtualenv_bin_path or default_bindir(), "black")

    log.debug("Black version {}".format(get_black_version(binary)))

    cmd_args = [binary]
    if not fix:
        cmd_args.append("--check")
    base_command = cmd_args + paths
    log.debug("Command: {}".format(" ".join(base_command)))
    return parse_issues(config, run_process(config, base_command), paths, log=log)


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs["root"]))

    return run_black(
        config,
        files,
        fix=fix,
        log=lintargs["log"],
        virtualenv_bin_path=lintargs.get("virtualenv_bin_path"),
    )
