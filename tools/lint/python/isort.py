# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import configparser
import os
import platform
import re
import signal
import subprocess
import sys

import mozpack.path as mozpath
from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozprocess import ProcessHandler

here = os.path.abspath(os.path.dirname(__file__))
ISORT_REQUIREMENTS_PATH = os.path.join(here, "isort_requirements.txt")

ISORT_INSTALL_ERROR = """
Unable to install correct version of isort
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(
    ISORT_REQUIREMENTS_PATH
)


def default_bindir():
    # We use sys.prefix to find executables as that gets modified with
    # virtualenv's activate_this.py, whereas sys.executable doesn't.
    if platform.system() == "Windows":
        return os.path.join(sys.prefix, "Scripts")
    else:
        return os.path.join(sys.prefix, "bin")


def parse_issues(config, output, *, log):
    would_sort = re.compile(
        "^ERROR: (.*?) Imports are incorrectly sorted and/or formatted.$", re.I
    )
    sorted = re.compile("^Fixing (.*)$", re.I)
    results = []
    for line in output:
        line = line.decode("utf-8")

        match = would_sort.match(line)
        if match:
            res = {"path": match.group(1)}
            results.append(result.from_config(config, **res))
            continue

        match = sorted.match(line)
        if match:
            res = {"path": match.group(1), "message": "sorted"}
            results.append(result.from_config(config, **res))
            continue

        log.debug("Unhandled line", line)
    return results


class IsortProcess(ProcessHandler):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs["stream"] = False
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(config, cmd):
    proc = IsortProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return proc.output


def setup(root, **lintargs):
    virtualenv_manager = lintargs["virtualenv_manager"]
    try:
        virtualenv_manager.install_pip_requirements(ISORT_REQUIREMENTS_PATH, quiet=True)
    except subprocess.CalledProcessError:
        print(ISORT_INSTALL_ERROR)
        return 1


def lint(paths, config, **lintargs):
    from isort import __version__ as isort_version

    binary = os.path.join(
        lintargs.get("virtualenv_bin_path") or default_bindir(), "isort"
    )

    log = lintargs["log"]
    root = lintargs["root"]

    log.debug("isort version {}".format(isort_version))

    cmd_args = [
        binary,
        "--resolve-all-configs",
        "--config-root",
        root,
    ]
    if not lintargs.get("fix"):
        cmd_args.append("--check-only")

    # We merge exclusion rules from .flake8 to avoid having to repeat the same exclusions twice.
    flake8_config_path = os.path.join(root, ".flake8")
    flake8_config = configparser.ConfigParser()
    flake8_config.read(flake8_config_path)
    config["exclude"].extend(
        mozpath.normpath(p.strip())
        for p in flake8_config.get("flake8", "exclude").split(",")
    )

    paths = list(expand_exclusions(paths, config, lintargs["root"]))
    if len(paths) == 0:
        return {"results": [], "fixed": 0}

    base_command = cmd_args + paths
    log.debug("Command: {}".format(" ".join(base_command)))

    output = run_process(config, base_command)

    results = parse_issues(config, output, log=log)

    fixed = sum(1 for issue in results if issue.message == "sorted")

    return {"results": results, "fixed": fixed}
