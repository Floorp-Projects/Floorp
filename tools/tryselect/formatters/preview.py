# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is intended to be called through fzf as a preview formatter.

from __future__ import absolute_import, print_function

import json
import os
from datetime import timedelta, datetime
import argparse


def process_args():
    argparser = argparse.ArgumentParser()
    argparser.add_argument('-d', '--durations-file', type=str, default=None)
    argparser.add_argument('-g', '--graph_cache', type=str, default=None)
    argparser.add_argument('tasklist', type=str)
    return argparser.parse_args()


def plain_data(tasklist):
    print("\n".join(sorted(s.strip("'") for s in tasklist.split())))


def find_dependencies(graph, task):
    dependencies = set()
    dependencies.add(task)
    for dep in graph.get(task, list()):
        dependencies.update(find_dependencies(graph, dep))
    return dependencies


def find_all_dependencies(graph, tasklist):
    full_deps = set()
    for task in tasklist:
        full_deps.update(find_dependencies(graph, task))

    # Since these have been asked for, they're not inherited dependencies.
    return sorted(full_deps - set(tasklist))


def find_dependency_durations(graph, task, current_duration, durations):
    current_duration += durations.get(task, 0.0)
    durations = [find_dependency_durations(graph, dep, current_duration, durations)
                 for dep in graph.get(task, list())]
    durations.append(current_duration)
    return max(durations)


def find_longest_path(graph, tasklist, durations):
    longest_paths = [find_dependency_durations(graph, task, 0.0, durations) for task in tasklist]
    return max(longest_paths)


def duration_data(durations_file, graph_cache_file, tasklist):
    tasklist = [t.strip("'") for t in tasklist.split()]
    with open(durations_file) as f:
        durations = json.load(f)
    durations = {d['name']: d['mean_duration_seconds'] for d in durations}

    graph = dict()
    if graph_cache_file:
        with open(graph_cache_file) as f:
            graph = json.load(f)
    dependencies = find_all_dependencies(graph, tasklist)
    longest_path = find_longest_path(graph, tasklist, durations)
    dependency_duration = 0.0
    for task in dependencies:
        dependency_duration += int(durations.get(task, 0.0))

    total_requested_duration = 0.0
    output = ""
    duration_width = 5  # show five numbers at most.

    max_columns = int(os.environ['FZF_PREVIEW_COLUMNS'])

    output = "{:>{width}}\n".format("Duration", width=max_columns)
    for task in tasklist:
        duration = int(durations.get(task, 0.0))
        total_requested_duration += duration
        output += "{:{align}{width}} {:{nalign}{nwidth}}s\n".format(
            task,
            duration,
            align='<',
            width=max_columns-(duration_width+2),  # 2: space and 's'
            nalign='>',
            nwidth=duration_width,
        )

    total_requested_duration = timedelta(seconds=total_requested_duration)
    total_dependency_duration = timedelta(seconds=dependency_duration)

    output += "\nSelected tasks take {}\n".format(total_requested_duration)
    output += "+{} dependencies, total {}\n".format(
        len(dependencies), total_dependency_duration + total_requested_duration)
    output += "Estimated finish in {} at {}".format(
        timedelta(seconds=int(longest_path)),
        (datetime.now()+timedelta(seconds=longest_path)).strftime("%H:%M"))
    print(output)


if __name__ == "__main__":
    args = process_args()
    if args.durations_file and os.path.isfile(args.durations_file):
        duration_data(args.durations_file, args.graph_cache, args.tasklist)
    else:
        plain_data(args.tasklist)
