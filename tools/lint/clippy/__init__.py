# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import bisect
import json
import os
import signal
import subprocess
import sys

import six
from mozlint import result
from mozlint.pathutils import expand_exclusions
from mozprocess import ProcessHandler


def in_sorted_list(l, x):
    i = bisect.bisect_left(l, x)
    return l[i] == x


def handle_clippy_msg(config, line, log, base_path, files):
    try:
        detail = json.loads(six.ensure_text(line))
        if "message" in detail:
            p = detail["target"]["src_path"]
            detail = detail["message"]
            if "level" in detail:
                if (
                    detail["level"] == "error" or detail["level"] == "failure-note"
                ) and not detail["code"]:
                    log.debug(
                        "Error outside of clippy."
                        "This means that the build failed. Therefore, skipping this"
                    )
                    log.debug("File = {} / Detail = {}".format(p, detail))
                    return
                # We are in a clippy warning
                if len(detail["spans"]) == 0:
                    # For some reason, at the end of the summary, we can
                    # get the following line
                    # {'rendered': 'warning: 5 warnings emitted\n\n', 'children':
                    # [], 'code': None, 'level': 'warning', 'message':
                    # '5 warnings emitted', 'spans': []}
                    # if this is the case, skip it
                    log.debug(
                        "Skipping the summary line {} for file {}".format(detail, p)
                    )
                    return

                l = detail["spans"][0]
                if not in_sorted_list(files, p):
                    return
                p = os.path.join(base_path, l["file_name"])
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
                return result.from_config(config, **res)

    except json.decoder.JSONDecodeError:
        log.debug("Could not parse the output:")
        log.debug("clippy output: {}".format(line))
        return


class clippyProcess(ProcessHandler):
    def __init__(self, *args, **kwargs):
        kwargs["stream"] = False
        ProcessHandler.__init__(self, *args, **kwargs)

    def run(self, *args, **kwargs):
        orig = signal.signal(signal.SIGINT, signal.SIG_IGN)
        ProcessHandler.run(self, *args, **kwargs)
        signal.signal(signal.SIGINT, orig)


def lint(paths, config, fix=None, **lintargs):
    files = list(expand_exclusions(paths, config, lintargs["root"]))
    files.sort()
    log = lintargs["log"]
    results = []
    mach_path = lintargs["root"] + "/mach"
    march_cargo_process = subprocess.Popen(
        [
            sys.executable,
            mach_path,
            "--log-no-times",
            "cargo",
            "clippy",
            "--",
            "--message-format=json",
        ],
        stdout=subprocess.PIPE,
        text=True,
    )
    for l in march_cargo_process.stdout:
        r = handle_clippy_msg(config, l, log, lintargs["root"], files)
        if r is not None:
            results.append(r)
    march_cargo_process.wait()

    if fix:
        log.error("Rust linting in mach does not support --fix")

    return results
