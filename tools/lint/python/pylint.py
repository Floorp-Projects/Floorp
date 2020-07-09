# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os

import signal

from mozprocess import ProcessHandler

from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozlint.util import pip

here = os.path.abspath(os.path.dirname(__file__))
PYLINT_REQUIREMENTS_PATH = os.path.join(here, "pylint_requirements.txt")

PYLINT_NOT_FOUND = """
Could not find pylint! Install pylint and try again.

    $ pip install -U --require-hashes -r {}
""".strip().format(
    PYLINT_REQUIREMENTS_PATH
)


PYLINT_INSTALL_ERROR = """
Unable to install correct version of pylint
Try to install it manually with:
    $ pip install -U --require-hashes -r {}
""".strip().format(
    PYLINT_REQUIREMENTS_PATH
)


class PylintProcess(ProcessHandler):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs["stream"] = False
        kwargs["universal_newlines"] = True
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def setup(root, **lintargs):
    if not pip.reinstall_program(PYLINT_REQUIREMENTS_PATH):
        print(PYLINT_INSTALL_ERROR)
        return 1


def get_pylint_binary():
    return "pylint"


def run_process(config, cmd):
    proc = PylintProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return proc.output


def parse_issues(log, config, issues_json, path):
    results = []

    try:
        issues = json.loads(issues_json)
    except json.decoder.JSONDecodeError:
        log.debug("Could not parse the output:")
        log.debug("pylint output: {}".format(issues_json))
        return []

    for issue in issues:
        res = {
            "path": issue["path"],
            "level": issue["type"],
            "lineno": issue["line"],
            "column": issue["column"],
            "message": issue["message"],
            "rule": issue["message-id"],
        }
        results.append(result.from_config(config, **res))
    return results


def lint(paths, config, **lintargs):
    log = lintargs["log"]

    binary = get_pylint_binary()

    log = lintargs["log"]
    paths = list(expand_exclusions(paths, config, lintargs["root"]))

    cmd_args = [binary]
    results = []

    # list from https://code.visualstudio.com/docs/python/linting#_pylint
    # And ignore a bit more elements
    cmd_args += [
        "-fjson",
        "--disable=all",
        "--enable=F,E,unreachable,duplicate-key,unnecessary-semicolon,global-variable-not-assigned,unused-variable,binary-op-exception,bad-format-string,anomalous-backslash-in-string,bad-open-mode,no-else-return",  # NOQA: E501
        "--disable=import-error,no-member",
    ]

    base_command = cmd_args + paths
    log.debug("Command: {}".format(" ".join(cmd_args)))
    output = " ".join(run_process(config, base_command))
    results = parse_issues(log, config, str(output), [])

    return results
