# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os
from datetime import datetime, timedelta

TASK_DURATION_CACHE = "task_duration_history.json"
GRAPH_QUANTILE_CACHE = "graph_quantile_cache.csv"
TASK_DURATION_TAG_FILE = "task_duration_tag.json"


def find_all_dependencies(graph, tasklist):
    all_dependencies = dict()

    def find_dependencies(task):
        dependencies = set()
        if task in all_dependencies:
            return all_dependencies[task]
        if task not in graph:
            # Don't add tasks (and so durations) for
            # things optimised out.
            return dependencies
        dependencies.add(task)
        for dep in graph.get(task, list()):
            all_dependencies[dep] = find_dependencies(dep)
            dependencies.update(all_dependencies[dep])
        return dependencies

    full_deps = set()
    for task in tasklist:
        full_deps.update(find_dependencies(task))

    # Since these have been asked for, they're not inherited dependencies.
    return sorted(full_deps - set(tasklist))


def find_longest_path(graph, tasklist, duration_data):
    dep_durations = dict()

    def find_dependency_durations(task):
        if task in dep_durations:
            return dep_durations[task]

        durations = [find_dependency_durations(dep) for dep in graph.get(task, list())]
        durations.append(0.0)
        md = max(durations) + duration_data.get(task, 0.0)
        dep_durations[task] = md
        return md

    longest_paths = [find_dependency_durations(task) for task in tasklist]
    # Default in case there are no tasks
    if longest_paths:
        return max(longest_paths)
    else:
        return 0


def determine_percentile(quantiles_file, duration):
    duration = duration.total_seconds()

    with open(quantiles_file) as f:
        f.readline()  # skip header
        boundaries = [float(l.strip()) for l in f.readlines()]

    boundaries.sort()
    for i, v in enumerate(boundaries):
        if duration < v:
            break
    # Estimate percentile from len(boundaries)-quantile
    return int(100 * i / len(boundaries))


def task_duration_data(cache_dir):
    with open(os.path.join(cache_dir, TASK_DURATION_CACHE)) as f:
        return json.load(f)


def duration_summary(graph_cache_file, tasklist, cache_dir):
    durations = task_duration_data(cache_dir)

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
    for task in tasklist:
        duration = int(durations.get(task, 0.0))
        total_requested_duration += duration
    output = dict()

    total_requested_duration = timedelta(seconds=total_requested_duration)
    total_dependency_duration = timedelta(seconds=dependency_duration)

    output["selected_duration"] = total_requested_duration
    output["dependency_duration"] = total_dependency_duration
    output["dependency_count"] = len(dependencies)
    output["selected_count"] = len(tasklist)

    percentile = None
    graph_quantile_cache = os.path.join(cache_dir, GRAPH_QUANTILE_CACHE)
    if os.path.isfile(graph_quantile_cache):
        percentile = determine_percentile(
            graph_quantile_cache, total_dependency_duration + total_requested_duration
        )
    if percentile:
        output["percentile"] = percentile

    output["wall_duration_seconds"] = timedelta(seconds=int(longest_path))
    output["eta_datetime"] = datetime.now() + timedelta(seconds=longest_path)

    output["task_durations"] = {
        task: int(durations.get(task, 0.0)) for task in tasklist
    }

    return output
