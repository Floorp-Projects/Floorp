#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, division, print_function

import six
import os
import sys
import json
import math
import glob

AWSY_PATH = os.path.dirname(os.path.realpath(__file__))
if AWSY_PATH not in sys.path:
    sys.path.append(AWSY_PATH)

import parse_about_memory

# A description of each checkpoint and the root path to it.
CHECKPOINTS = [
    {"name": "Fresh start", "path": "memory-report-Start-0.json.gz"},
    {"name": "Fresh start [+30s]", "path": "memory-report-StartSettled-0.json.gz"},
    {"name": "After tabs open", "path": "memory-report-TabsOpen-4.json.gz"},
    {
        "name": "After tabs open [+30s]",
        "path": "memory-report-TabsOpenSettled-4.json.gz",
    },
    {
        "name": "After tabs open [+30s, forced GC]",
        "path": "memory-report-TabsOpenForceGC-4.json.gz",
    },
    {
        "name": "Tabs closed extra processes",
        "path": "memory-report-TabsClosedExtraProcesses-4.json.gz",
    },
    {"name": "Tabs closed", "path": "memory-report-TabsClosed-4.json.gz"},
    {"name": "Tabs closed [+30s]", "path": "memory-report-TabsClosedSettled-4.json.gz"},
    {
        "name": "Tabs closed [+30s, forced GC]",
        "path": "memory-report-TabsClosedForceGC-4.json.gz",
    },
]

# A description of each perfherder suite and the path to its values.
PERF_SUITES = [
    {"name": "Resident Memory", "node": "resident"},
    {"name": "Explicit Memory", "node": "explicit/"},
    {"name": "Heap Unclassified", "node": "explicit/heap-unclassified"},
    {"name": "JS", "node": "js-main-runtime/"},
    {"name": "Images", "node": "explicit/images/"},
]


def median(values):
    sorted_ = sorted(values)
    # pylint --py3k W1619
    med = int(len(sorted_) / 2)

    if len(sorted_) % 2:
        return sorted_[med]
    # pylint --py3k W1619
    return (sorted_[med - 1] + sorted_[med]) / 2


def update_checkpoint_paths(checkpoint_files, checkpoints):
    """
    Updates checkpoints with memory report file fetched in data_path
    :param checkpoint_files: list of files in data_path
    :param checkpoints: The checkpoints to update the path of.
    """
    target_path = [
        ["Start-", 0],
        ["StartSettled-", 0],
        ["TabsOpen-", -1],
        ["TabsOpenSettled-", -1],
        ["TabsOpenForceGC-", -1],
        ["TabsClosedExtraProcesses-", -1],
        ["TabsClosed-", -1],
        ["TabsClosedSettled-", -1],
        ["TabsClosedForceGC-", -1],
    ]
    for i in range(len(target_path)):
        (name, idx) = target_path[i]
        paths = sorted([x for x in checkpoint_files if name in x])
        if paths:
            indices = [i for i, x in enumerate(checkpoints) if name in x["path"]]
            if indices:
                checkpoints[indices[0]]["path"] = paths[idx]
            else:
                print("found files but couldn't find {}".format(name))


def create_suite(
    name, node, data_path, checkpoints=CHECKPOINTS, alertThreshold=None, extra_opts=None
):
    """
    Creates a suite suitable for adding to a perfherder blob. Calculates the
    geometric mean of the checkpoint values and adds that to the suite as
    well.

    :param name: The name of the suite.
    :param node: The path of the data node to extract data from.
    :param data_path: The directory to retrieve data from.
    :param checkpoints: Which checkpoints to include.
    :param alertThreshold: The percentage of change that triggers an alert.
    """
    suite = {"name": name, "subtests": [], "lowerIsBetter": True, "unit": "bytes"}

    if alertThreshold:
        suite["alertThreshold"] = alertThreshold

    opts = []
    if extra_opts:
        opts.extend(extra_opts)

    # The stylo attributes override each other.
    stylo_opt = None
    if "STYLO_FORCE_ENABLED" in os.environ and os.environ["STYLO_FORCE_ENABLED"]:
        stylo_opt = "stylo"
    if "STYLO_THREADS" in os.environ and os.environ["STYLO_THREADS"] == "1":
        stylo_opt = "stylo-sequential"

    if stylo_opt:
        opts.append(stylo_opt)

    if "DMD" in os.environ and os.environ["DMD"]:
        opts.append("dmd")

    if extra_opts:
        suite["extraOptions"] = opts

    update_checkpoint_paths(
        glob.glob(os.path.join(data_path, "memory-report*")), checkpoints
    )

    total = 0
    for checkpoint in checkpoints:
        memory_report_path = os.path.join(data_path, checkpoint["path"])

        name_filter = checkpoint.get("name_filter", None)
        if checkpoint.get("median"):
            process = median
        else:
            process = sum

        if node != "resident":
            totals = parse_about_memory.calculate_memory_report_values(
                memory_report_path, node, name_filter
            )
            value = process(totals.values())
        else:
            # For "resident" we really want RSS of the chrome ("Main") process
            # and USS of the child processes. We'll still call it resident
            # for simplicity (it's nice to be able to compare RSS of non-e10s
            # with RSS + USS of e10s).
            totals_rss = parse_about_memory.calculate_memory_report_values(
                memory_report_path, node, ["Main"]
            )
            totals_uss = parse_about_memory.calculate_memory_report_values(
                memory_report_path, "resident-unique"
            )
            value = list(totals_rss.values())[0] + sum(
                [v for k, v in six.iteritems(totals_uss) if "Main" not in k]
            )

        subtest = {
            "name": checkpoint["name"],
            "value": value,
            "lowerIsBetter": True,
            "unit": "bytes",
        }
        suite["subtests"].append(subtest)
        total += math.log(subtest["value"])

    # Add the geometric mean. For more details on the calculation see:
    #   https://en.wikipedia.org/wiki/Geometric_mean#Relationship_with_arithmetic_mean_of_logarithms
    # pylint --py3k W1619
    suite["value"] = math.exp(total / len(checkpoints))

    return suite


def create_perf_data(
    data_path, perf_suites=PERF_SUITES, checkpoints=CHECKPOINTS, extra_opts=None
):
    """
    Builds up a performance data blob suitable for submitting to perfherder.
    """
    if ("GCOV_PREFIX" in os.environ) or ("JS_CODE_COVERAGE_OUTPUT_DIR" in os.environ):
        print(
            "Code coverage is being collected, performance data will not be gathered."
        )
        return {}

    perf_blob = {"framework": {"name": "awsy"}, "suites": []}

    for suite in perf_suites:
        perf_blob["suites"].append(
            create_suite(
                suite["name"],
                suite["node"],
                data_path,
                checkpoints,
                suite.get("alertThreshold"),
                extra_opts,
            )
        )

    return perf_blob


if __name__ == "__main__":
    args = sys.argv[1:]
    if not args:
        print("Usage: process_perf_data.py data_path")
        sys.exit(1)

    # Determine which revisions we need to process.
    data_path = args[0]
    perf_blob = create_perf_data(data_path)
    print("PERFHERDER_DATA: {}".format(json.dumps(perf_blob)))

    sys.exit(0)
