#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


# Firefox about:memory log parser.

import argparse
from collections import defaultdict
import gzip
import json


def path_total(data, path):
    """
    Calculates the sum for the given data point path and its children. If
    path does not end with a '/' then only the value for the exact path is
    returned.
    """
    totals = defaultdict(int)
    totals_heap = defaultdict(int)
    totals_heap_allocated = defaultdict(int)

    discrete = not path.endswith('/')

    def match(value):
      if discrete:
        return value == path
      else:
        return value.startswith(path)

    for report in data["reports"]:
        if report["kind"] == 1 and report["path"].startswith("explicit/"):
            totals_heap[report["process"]] += report["amount"]

        if match(report["path"]):
            totals[report["process"]] += report["amount"]
            if report["kind"] == 1:
                totals_heap[report["process"]] += report["amount"]
        elif report["path"] == "heap-allocated":
            totals_heap_allocated[report["process"]] = report["amount"]

    if path == "explicit/":
        for k, v in totals_heap.items():
            if k in totals_heap_allocated:
                heap_unclassified = totals_heap_allocated[k] - totals_heap[k]
                totals[k] += heap_unclassified
    elif path == "explicit/heap-unclassified":
        for k, v in totals_heap.items():
            if k in totals_heap_allocated:
                totals[k] = totals_heap_allocated[k] - totals_heap[k]

    return totals


def calculate_memory_report_values(memory_report_path, data_point_path,
                                   process_name=None):
    """
    Opens the given memory report file and calculates the value for the given
    data point.

    :param memory_report_path: Path to the memory report file to parse.
    :param data_point_path: Path of the data point to calculate in the memory
     report, ie: 'explicit/heap-unclassified'.
    :param process_name: Name of process to limit reports to. ie 'Main'
    """
    try:
        with open(memory_report_path) as f:
            data = json.load(f)
    except ValueError, e:
        # Check if the file is gzipped.
        with gzip.open(memory_report_path, 'rb') as f:
            data = json.load(f)

    totals = path_total(data, data_point_path)

    # If a process name is provided, restricted output to processes matching
    # that name.
    if process_name:
        for k in totals.keys():
            if not process_name in k:
                del totals[k]

    return totals


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
            description='Extract data points from about:memory reports')
    parser.add_argument('report', action='store',
                        help='Path to a memory report file.')
    parser.add_argument('prefix', action='store',
                        help='Prefix of data point to measure. If the prefix does not end in a \'/\' then an exact match is made.')
    parser.add_argument('--proc-filter', action='store', default=None,
                        help='Process name filter. If not provided all processes will be included.')

    args = parser.parse_args()
    totals = calculate_memory_report_values(
                    args.report, args.prefix, args.proc_filter)

    sorted_totals = sorted(totals.iteritems(), key=lambda(k,v): (-v,k))
    for (k, v) in sorted_totals:
        if v:
            print "{0}\t".format(k),
    print ""
    for (k, v) in sorted_totals:
        if v:
            print "{0}\t".format(v),
    print ""
