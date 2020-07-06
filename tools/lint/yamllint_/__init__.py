# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import re
import os
import subprocess
from collections import defaultdict

from mozfile import which

from mozlint import result
from mozlint.pathutils import get_ancestors_by_name
from mozlint.util.implementation import LintProcess


YAMLLINT_FORMAT_REGEX = re.compile("(.*):(.*):(.*): \[(error|warning)\] (.*) \((.*)\)$")

results = []


class YAMLLintProcess(LintProcess):
    def process_line(self, line):
        try:
            match = YAMLLINT_FORMAT_REGEX.match(line)
            abspath, line, col, level, message, code = match.groups()
        except AttributeError:
            print("Unable to match yaml regex against output: {}".format(line))
            return

        res = {
            "path": os.path.relpath(str(abspath), self.config["root"]),
            "message": str(message),
            "level": "error",
            "lineno": line,
            "column": col,
            "rule": code,
        }

        results.append(result.from_config(self.config, **res))


def get_yamllint_binary(mc_root):
    """
    Returns the path of the first yamllint binary available
    if not found returns None
    """
    binary = os.environ.get("YAMLLINT")
    if binary:
        return binary

    # yamllint is vendored in mozilla-central: let's use this
    # if no environment variable is found.
    return os.path.join(mc_root, "third_party", "python", "yamllint", "yamllint")


def get_yamllint_version(binary):
    return subprocess.check_output(
        [which("python"), binary, "--version"],
        universal_newlines=True,
        stderr=subprocess.STDOUT,
    )


def _run_pip(*args):
    """
    Helper function that runs pip with subprocess
    """
    try:
        subprocess.check_output(["pip"] + list(args), stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError as e:
        print(e.output)
        return False


def run_process(config, cmd):
    proc = YAMLLintProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()


def gen_yamllint_args(cmdargs, paths=None, conf_file=None):
    args = cmdargs[:]
    if isinstance(paths, str):
        paths = [paths]
    if conf_file and conf_file != "default":
        return args + ["-c", conf_file] + paths
    return args + paths


def lint(files, config, **lintargs):
    log = lintargs["log"]

    binary = get_yamllint_binary(lintargs["root"])

    log.debug("Version: {}".format(get_yamllint_version(binary)))

    cmdargs = [which("python"), binary, "-f", "parsable"]
    log.debug("Command: {}".format(" ".join(cmdargs)))

    config = config.copy()
    config["root"] = lintargs["root"]

    # Run any paths with a .yamllint file in the directory separately so
    # it gets picked up. This means only .yamllint files that live in
    # directories that are explicitly included will be considered.
    paths_by_config = defaultdict(list)
    for f in files:
        conf_files = get_ancestors_by_name(".yamllint", f, config["root"])
        paths_by_config[conf_files[0] if conf_files else "default"].append(f)

    for conf_file, paths in paths_by_config.items():
        run_process(
            config, gen_yamllint_args(cmdargs, conf_file=conf_file, paths=paths)
        )

    return results
