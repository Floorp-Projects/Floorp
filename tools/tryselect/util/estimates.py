# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
import requests
import json
from datetime import datetime, timedelta


TASK_DURATION_URL = 'https://storage.googleapis.com/mozilla-mach-data/task_duration_history.json'
GRAPH_QUANTILES_URL = 'https://storage.googleapis.com/mozilla-mach-data/machtry_quantiles.csv'
TASK_DURATION_CACHE = 'task_duration_history.json'
GRAPH_QUANTILE_CACHE = 'graph_quantile_cache.csv'
TASK_DURATION_TAG_FILE = 'task_duration_tag.json'


def check_downloaded_history(tag_file, duration_cache, quantile_cache):
    if not os.path.isfile(tag_file):
        return False

    try:
        with open(tag_file) as f:
            duration_tags = json.load(f)
        download_date = datetime.strptime(duration_tags.get('download_date'), '%Y-%M-%d')
        if download_date < datetime.now() - timedelta(days=30):
            return False
    except (IOError, ValueError):
        return False

    if not os.path.isfile(duration_cache):
        return False
    if not os.path.isfile(quantile_cache):
        return False

    return True


def download_task_history_data(cache_dir):
    """Fetch task duration data exported from BigQuery."""
    task_duration_cache = os.path.join(cache_dir, TASK_DURATION_CACHE)
    task_duration_tag_file = os.path.join(cache_dir, TASK_DURATION_TAG_FILE)
    graph_quantile_cache = os.path.join(cache_dir, GRAPH_QUANTILE_CACHE)

    if check_downloaded_history(task_duration_tag_file, task_duration_cache, graph_quantile_cache):
        return

    try:
        os.unlink(task_duration_tag_file)
        os.unlink(task_duration_cache)
        os.unlink(graph_quantile_cache)
    except OSError:
        print("No existing task history to clean up.")

    try:
        r = requests.get(TASK_DURATION_URL, stream=True)
    except requests.exceptions.RequestException as exc:
        # This is fine, the durations just won't be in the preview window.
        print("Error fetching task duration cache from {}: {}".format(TASK_DURATION_URL, exc))
        return

    # The data retrieved from google storage is a newline-separated
    # list of json entries, which Python's json module can't parse.
    duration_data = list()
    for line in r.content.splitlines():
        duration_data.append(json.loads(line))

    with open(task_duration_cache, 'w') as f:
        json.dump(duration_data, f, indent=4)

    try:
        r = requests.get(GRAPH_QUANTILES_URL, stream=True)
    except requests.exceptions.RequestException as exc:
        # This is fine, the percentile just won't be in the preview window.
        print("Error fetching task group percentiles from {}: {}".format(GRAPH_QUANTILES_URL, exc))
        return

    with open(graph_quantile_cache, 'w') as f:
        f.write(r.content)

    with open(task_duration_tag_file, 'w') as f:
        json.dump({
            'download_date': datetime.now().strftime('%Y-%m-%d')
            }, f, indent=4)


def make_trimmed_taskgraph_cache(graph_cache, dep_cache, target_file=None):
    """Trim the taskgraph cache used for dependencies.

    Speeds up the fzf preview window to less human-perceptible
    ranges."""
    if not os.path.isfile(graph_cache):
        return

    target_task_set = set()
    if target_file:
        with open(target_file) as f:
            target_task_set = set(json.load(f).keys())

    with open(graph_cache) as f:
        graph = json.load(f)
    graph = {
        name: list(defn['dependencies'].values())
        for name, defn in graph.items()
        if name in target_task_set
    }
    with open(dep_cache, 'w') as f:
        json.dump(graph, f, indent=4)


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

        durations = [find_dependency_durations(dep)
                     for dep in graph.get(task, list())]
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


def determine_quantile(quantiles_file, duration):

    duration = duration.total_seconds()

    with open(quantiles_file) as f:
        f.readline()  # skip header
        boundaries = [float(l.strip()) for l in f.readlines()]
        boundaries.sort()

    for i, v in enumerate(boundaries):
        if duration < v:
            break
    # In case we weren't given 100 elements
    return int(100 * i / len(boundaries))


def task_duration_data(cache_dir):
    with open(os.path.join(cache_dir, TASK_DURATION_CACHE)) as f:
        durations = json.load(f)
    return {d['name']: d['mean_duration_seconds'] for d in durations}


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

    quantile = None
    graph_quantile_cache = os.path.join(cache_dir, GRAPH_QUANTILE_CACHE)
    if os.path.isfile(graph_quantile_cache):
        quantile = 100 - determine_quantile(graph_quantile_cache,
                                            total_dependency_duration + total_requested_duration)
    if quantile:
        output['quantile'] = quantile

    output["wall_duration_seconds"] = timedelta(seconds=int(longest_path))
    output["eta_datetime"] = datetime.now()+timedelta(seconds=longest_path)
    # (datetime.now()+timedelta(seconds=longest_path)).strftime("%H:%M")

    return output
