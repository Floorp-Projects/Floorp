# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import json
import os
import sys

from mozboot.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozversioncontrol import get_repository_object, MissingVCSExtension
from .util.manage_estimates import (
    download_task_history_data,
    make_trimmed_taskgraph_cache
)
from .util.estimates import duration_summary

GIT_CINNABAR_NOT_FOUND = """
Could not detect `git-cinnabar`.

The `mach try` command requires git-cinnabar to be installed when
pushing from git. Please install it by running:

    $ ./mach vcs-setup
""".lstrip()

HG_PUSH_TO_TRY_NOT_FOUND = """
Could not detect `push-to-try`.

The `mach try` command requires the push-to-try extension enabled
when pushing from hg. Please install it by running:

    $ ./mach vcs-setup
""".lstrip()

VCS_NOT_FOUND = """
Could not detect version control. Only `hg` or `git` are supported.
""".strip()

UNCOMMITTED_CHANGES = """
ERROR please commit changes before continuing
""".strip()

MAX_HISTORY = 10

here = os.path.abspath(os.path.dirname(__file__))
build = MozbuildObject.from_environment(cwd=here)
vcs = get_repository_object(build.topsrcdir)

history_path = os.path.join(get_state_dir(srcdir=True), 'history', 'try_task_configs.json')


def write_task_config(try_task_config):
    config_path = os.path.join(vcs.path, 'try_task_config.json')
    with open(config_path, 'w') as fh:
        json.dump(try_task_config, fh, indent=4, separators=(',', ': '), sort_keys=True)
        fh.write('\n')
    return config_path


def write_task_config_history(msg, try_task_config):
    if not os.path.isfile(history_path):
        if not os.path.isdir(os.path.dirname(history_path)):
            os.makedirs(os.path.dirname(history_path))
        history = []
    else:
        with open(history_path, 'r') as fh:
            history = fh.read().strip().splitlines()

    history.insert(0, json.dumps([msg, try_task_config]))
    history = history[:MAX_HISTORY]
    with open(history_path, 'w') as fh:
        fh.write('\n'.join(history))


def check_working_directory(push=True):
    if not push:
        return

    if not vcs.working_directory_clean():
        print(UNCOMMITTED_CHANGES)
        sys.exit(1)


def generate_try_task_config(method, labels, try_config=None, routes=None):
    try_task_config = try_config or {}
    try_task_config.setdefault('env', {})['TRY_SELECTOR'] = method
    try_task_config.update({
        'version': 1,
        'tasks': sorted(labels),
    })
    if routes:
        try_task_config["routes"] = routes

    return try_task_config


def task_labels_from_try_config(try_task_config):
    return try_task_config.get("tasks", list())


def display_push_estimates(try_task_config):
    cache_dir = os.path.join(get_state_dir(srcdir=True), 'cache', 'taskgraph')

    graph_cache = None
    dep_cache = None
    target_file = None
    for graph_cache_file in ["target_task_graph", "full_task_graph"]:
        graph_cache = os.path.join(cache_dir, graph_cache_file)
        if os.path.isfile(graph_cache):
            dep_cache = graph_cache.replace("task_graph", "task_dependencies")
            target_file = graph_cache.replace("task_graph", "task_set")
            break

    if not dep_cache:
        return

    download_task_history_data(cache_dir=cache_dir)
    make_trimmed_taskgraph_cache(graph_cache, dep_cache, target_file=target_file)

    durations = duration_summary(
        dep_cache, task_labels_from_try_config(try_task_config), cache_dir)

    print("estimates: Runs {} tasks ({} selected, {} dependencies)".format(
        durations["dependency_count"] + durations["selected_count"],
        durations["selected_count"],
        durations["dependency_count"])
    )
    print("estimates: Total task duration {}".format(
        durations["dependency_duration"] + durations["selected_duration"]
    ))
    print("estimates: In the {}% percentile".format(durations["quantile"]))
    print("estimates: Should take about {} (Finished around {})".format(
        durations["wall_duration_seconds"],
        durations["eta_datetime"].strftime("%Y-%m-%d %H:%M"))
    )


def push_to_try(method, msg, try_task_config=None,
                push=True, closed_tree=False, files_to_change=None):
    check_working_directory(push)

    if try_task_config and method not in ('auto', 'empty'):
        display_push_estimates(try_task_config)

    # Format the commit message
    closed_tree_string = " ON A CLOSED TREE" if closed_tree else ""
    commit_message = ('%s%s\n\nPushed via `mach try %s`' %
                      (msg, closed_tree_string, method))

    config_path = None
    changed_files = []
    if try_task_config:
        if push and method not in ('again', 'auto', 'empty'):
            write_task_config_history(msg, try_task_config)
        config_path = write_task_config(try_task_config)
        changed_files.append(config_path)

    if files_to_change:
        for path, content in files_to_change.items():
            path = os.path.join(vcs.path, path)
            with open(path, 'w') as fh:
                fh.write(content)
            changed_files.append(path)

    try:
        if not push:
            print("Commit message:")
            print(commit_message)
            if config_path:
                print("Calculated try_task_config.json:")
                with open(config_path) as fh:
                    print(fh.read())
            return

        for path in changed_files:
            vcs.add_remove_files(path)

        try:
            vcs.push_to_try(commit_message)
        except MissingVCSExtension as e:
            if e.ext == 'push-to-try':
                print(HG_PUSH_TO_TRY_NOT_FOUND)
            elif e.ext == 'cinnabar':
                print(GIT_CINNABAR_NOT_FOUND)
            else:
                raise
            sys.exit(1)
    finally:
        if config_path and os.path.isfile(config_path):
            os.remove(config_path)
