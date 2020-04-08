# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import signal
import six
import subprocess

from mozfile import which
from mozlint import result
from mozlint.pathutils import get_ancestors_by_name
from mozprocess import ProcessHandler


CLIPPY_NOT_FOUND = """
Could not find clippy! Install clippy and try again.

    $ rustup component add clippy

And make sure that it is in the PATH
""".strip()


CARGO_NOT_FOUND = """
Could not find cargo! Install cargo.

And make sure that it is in the PATH
""".strip()


def parse_issues(log, config, issues, path, onlyIn):
    results = []
    for issue in issues:

        try:
            detail = json.loads(six.ensure_text(issue))
            if "message" in detail:
                p = detail['target']['src_path']
                detail = detail["message"]
                if "level" in detail:
                    if ((detail["level"] == "error" or detail["level"] == "failure-note")
                        and not detail["code"]):
                        log.debug("Error outside of clippy."
                                  "This means that the build failed. Therefore, skipping this")
                        log.debug("File = {} / Detail = {}".format(p, detail))
                        continue
                    # We are in a clippy warning
                    l = detail["spans"][0]
                    if onlyIn and onlyIn not in p:
                        # Case when we have a .rs in the include list in the yaml file
                        log.debug("{} is not part of the list of files '{}'".format(p, onlyIn))
                        continue
                    res = {
                        "path": p,
                        "level": detail["level"],
                        "lineno": l["line_start"],
                        "column": l["column_start"],
                        "message": detail["message"],
                        "hint": detail["rendered"],
                        "rule": detail["code"]["code"],
                        "lineoffset": l["line_end"] - l["line_start"],
                    }
                    results.append(result.from_config(config, **res))

        except json.decoder.JSONDecodeError:
            log.debug("Could not parse the output:")
            log.debug("clippy output: {}".format(issue))
            continue

    return results


def get_cargo_binary(log):
    """
    Returns the path of the first rustfmt binary available
    if not found returns None
    """
    cargo_home = os.environ.get("CARGO_HOME")
    if cargo_home:
        log.debug("Found CARGO_HOME in {}".format(cargo_home))
        cargo_bin = os.path.join(cargo_home, "bin", "cargo")
        if os.path.exists(cargo_bin):
            return cargo_bin
        log.debug("Did not find {} in CARGO_HOME".format(cargo_bin))
        return None
    return which("cargo")


def is_clippy_installed(binary):
    """
    Check if we are running the deprecated rustfmt
    """
    try:
        output = subprocess.check_output(
            [binary, "clippy", "--help"],
            stderr=subprocess.STDOUT,
            universal_newlines=True,
        )
    except subprocess.CalledProcessError as e:
        output = e.output

    if "Checks a package" in output:
        return True

    return False


class clippyProcess(ProcessHandler):
    def __init__(self, config, *args, **kwargs):
        self.config = config
        kwargs["stream"] = False
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def run_process(log, config, cmd):
    log.debug("Command: {}".format(cmd))
    proc = clippyProcess(config, cmd)
    proc.run()
    try:
        proc.wait()
    except KeyboardInterrupt:
        proc.kill()

    return proc.output


def lint(paths, config, fix=None, **lintargs):
    log = lintargs["log"]
    cargo = get_cargo_binary(log)

    if not cargo:
        print(CARGO_NOT_FOUND)
        if 'MOZ_AUTOMATION' in os.environ:
            return 1
        return []

    if not is_clippy_installed(cargo):
        print(CLIPPY_NOT_FOUND)
        if 'MOZ_AUTOMATION' in os.environ:
            return 1
        return []

    cmd_args_clean = [cargo]
    cmd_args_clean.append("clean")

    cmd_args_common = ["--manifest-path"]
    cmd_args_clippy = [
        cargo,
        'clippy',
        '--message-format=json',
    ]

    results = []

    for p in paths:
        # Quick sanity check of the paths
        if p.endswith("Cargo.toml"):
            print("Error: expects a directory or a rs file")
            print("Found {}".format(p))
            return 1

    for p in paths:
        onlyIn = []
        path_conf = p
        log.debug("Path = {}".format(p))
        if os.path.isfile(p):
            # We are dealing with a file. We remove the filename from the path
            # to find the closest Cargo file
            # We also store the name of the file to be able to filter out other
            # files built by the cargo
            p = os.path.dirname(p)
            onlyIn = path_conf

        if os.path.isdir(p):
            # Sometimes, clippy reports issues from other crates
            # Make sure that we don't display that either
            onlyIn = p

        cargo_files = get_ancestors_by_name('Cargo.toml', p, lintargs['root'])
        p = cargo_files[0]

        log.debug("Path translated to = {}".format(p))
        # Needs clean because of https://github.com/rust-lang/rust-clippy/issues/2604
        clean_command = cmd_args_clean + cmd_args_common + [p]
        run_process(log, config, clean_command)

        # Create the actual clippy command
        base_command = cmd_args_clippy + cmd_args_common + [p]
        output = run_process(log, config, base_command)

        results += parse_issues(log, config, output, p, onlyIn)

    return sorted(results, key=lambda issue: issue.path)
