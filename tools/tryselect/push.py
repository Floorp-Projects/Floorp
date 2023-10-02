# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import os
import sys
import traceback

import six
from mach.util import get_state_dir
from mozbuild.base import MozbuildObject
from mozversioncontrol import MissingVCSExtension, get_repository_object

from .lando import push_to_lando_try
from .util.estimates import duration_summary
from .util.manage_estimates import (
    download_task_history_data,
    make_trimmed_taskgraph_cache,
)

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

history_path = os.path.join(
    get_state_dir(specific_to_topsrcdir=True), "history", "try_task_configs.json"
)


def write_task_config(try_task_config):
    config_path = os.path.join(vcs.path, "try_task_config.json")
    with open(config_path, "w") as fh:
        json.dump(try_task_config, fh, indent=4, separators=(",", ": "), sort_keys=True)
        fh.write("\n")
    return config_path


def write_task_config_history(msg, try_task_config):
    if not os.path.isfile(history_path):
        if not os.path.isdir(os.path.dirname(history_path)):
            os.makedirs(os.path.dirname(history_path))
        history = []
    else:
        with open(history_path) as fh:
            history = fh.read().strip().splitlines()

    history.insert(0, json.dumps([msg, try_task_config]))
    history = history[:MAX_HISTORY]
    with open(history_path, "w") as fh:
        fh.write("\n".join(history))


def check_working_directory(push=True):
    if not push:
        return

    if not vcs.working_directory_clean():
        print(UNCOMMITTED_CHANGES)
        sys.exit(1)


def generate_try_task_config(method, labels, try_config=None, routes=None):
    try_task_config = try_config or {}
    try_task_config.setdefault("env", {})["TRY_SELECTOR"] = method
    try_task_config.update(
        {
            "version": 1,
            "tasks": sorted(labels),
        }
    )
    if routes:
        try_task_config["routes"] = routes

    return try_task_config


def task_labels_from_try_config(try_task_config):
    if try_task_config["version"] == 2:
        parameters = try_task_config.get("parameters", {})
        if parameters.get("try_mode") == "try_task_config":
            return parameters["try_task_config"]["tasks"]
        else:
            return None
    elif try_task_config["version"] == 1:
        return try_task_config.get("tasks", list())
    else:
        return None


def display_push_estimates(try_task_config):
    task_labels = task_labels_from_try_config(try_task_config)
    if task_labels is None:
        return

    cache_dir = os.path.join(
        get_state_dir(specific_to_topsrcdir=True), "cache", "taskgraph"
    )

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

    durations = duration_summary(dep_cache, task_labels, cache_dir)

    print(
        "estimates: Runs {} tasks ({} selected, {} dependencies)".format(
            durations["dependency_count"] + durations["selected_count"],
            durations["selected_count"],
            durations["dependency_count"],
        )
    )
    print(
        "estimates: Total task duration {}".format(
            durations["dependency_duration"] + durations["selected_duration"]
        )
    )
    if "percentile" in durations:
        print(
            "estimates: In the top {}% of durations".format(
                100 - durations["percentile"]
            )
        )
    print(
        "estimates: Should take about {} (Finished around {})".format(
            durations["wall_duration_seconds"],
            durations["eta_datetime"].strftime("%Y-%m-%d %H:%M"),
        )
    )


def push_to_try(
    method,
    msg,
    try_task_config=None,
    stage_changes=False,
    dry_run=False,
    closed_tree=False,
    files_to_change=None,
    allow_log_capture=False,
    push_to_lando=False,
):
    push = not stage_changes and not dry_run
    check_working_directory(push)

    if try_task_config and method not in ("auto", "empty"):
        try:
            display_push_estimates(try_task_config)
        except Exception:
            traceback.print_exc()
            print("warning: unable to display push estimates")

    # Format the commit message
    closed_tree_string = " ON A CLOSED TREE" if closed_tree else ""
    commit_message = "{}{}\n\nPushed via `mach try {}`".format(
        msg,
        closed_tree_string,
        method,
    )

    config_path = None
    changed_files = []
    if try_task_config:
        if push and method not in ("again", "auto", "empty"):
            write_task_config_history(msg, try_task_config)
        config_path = write_task_config(try_task_config)
        changed_files.append(config_path)

    if (push or stage_changes) and files_to_change:
        for path, content in files_to_change.items():
            path = os.path.join(vcs.path, path)
            with open(path, "wb") as fh:
                fh.write(six.ensure_binary(content))
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

        vcs.add_remove_files(*changed_files)

        try:
            if push_to_lando:
                push_to_lando_try(vcs, commit_message)
            else:
                vcs.push_to_try(commit_message, allow_log_capture=allow_log_capture)
        except MissingVCSExtension as e:
            if e.ext == "push-to-try":
                print(HG_PUSH_TO_TRY_NOT_FOUND)
            elif e.ext == "cinnabar":
                print(GIT_CINNABAR_NOT_FOUND)
            else:
                raise
            sys.exit(1)
    finally:
        if config_path and os.path.isfile(config_path):
            os.remove(config_path)
