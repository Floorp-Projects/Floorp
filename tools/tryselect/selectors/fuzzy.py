# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os
import sys
from pathlib import PurePath

from gecko_taskgraph.target_tasks import filter_by_uncommon_try_tasks
from mach.util import get_state_dir

from ..cli import BaseTryParser
from ..push import check_working_directory, generate_try_task_config, push_to_try
from ..tasks import filter_tasks_by_paths, generate_tasks
from ..util.fzf import (
    FZF_NOT_FOUND,
    PREVIEW_SCRIPT,
    format_header,
    fzf_bootstrap,
    fzf_shortcuts,
    run_fzf,
)
from ..util.manage_estimates import (
    download_task_history_data,
    make_trimmed_taskgraph_cache,
)


class FuzzyParser(BaseTryParser):
    name = "fuzzy"
    arguments = [
        [
            ["-q", "--query"],
            {
                "metavar": "STR",
                "action": "append",
                "default": [],
                "help": "Use the given query instead of entering the selection "
                "interface. Equivalent to typing <query><ctrl-a><enter> "
                "from the interface. Specifying multiple times schedules "
                "the union of computed tasks.",
            },
        ],
        [
            ["-i", "--interactive"],
            {
                "action": "store_true",
                "default": False,
                "help": "Force running fzf interactively even when using presets or "
                "queries with -q/--query.",
            },
        ],
        [
            ["-x", "--and"],
            {
                "dest": "intersection",
                "action": "store_true",
                "default": False,
                "help": "When specifying queries on the command line with -q/--query, "
                "use the intersection of tasks rather than the union. This is "
                "especially useful for post filtering presets.",
            },
        ],
        [
            ["-e", "--exact"],
            {
                "action": "store_true",
                "default": False,
                "help": "Enable exact match mode. Terms will use an exact match "
                "by default, and terms prefixed with ' will become fuzzy.",
            },
        ],
        [
            ["-u", "--update"],
            {
                "action": "store_true",
                "default": False,
                "help": "Update fzf before running.",
            },
        ],
        [
            ["-s", "--show-estimates"],
            {
                "action": "store_true",
                "default": False,
                "help": "Show task duration estimates.",
            },
        ],
        [
            ["--disable-target-task-filter"],
            {
                "action": "store_true",
                "default": False,
                "help": "Some tasks run on mozilla-central but are filtered out "
                "of the default list due to resource constraints. This flag "
                "disables this filtering.",
            },
        ],
        [
            ["--show-chunk-numbers"],
            {
                "action": "store_true",
                "default": False,
                "help": "Chunk numbers are hidden to simplify the selection. This flag "
                "makes them appear again.",
            },
        ],
    ]
    common_groups = ["push", "task", "preset"]
    task_configs = [
        "artifact",
        "browsertime",
        "chemspill-prio",
        "disable-pgo",
        "env",
        "gecko-profile",
        "path",
        "pernosco",
        "rebuild",
        "routes",
        "worker-overrides",
    ]


def run(
    update=False,
    query=None,
    intersect_query=None,
    try_config=None,
    full=False,
    parameters=None,
    save_query=False,
    stage_changes=False,
    dry_run=False,
    message="{msg}",
    test_paths=None,
    exact=False,
    closed_tree=False,
    show_estimates=False,
    disable_target_task_filter=False,
    push_to_lando=False,
    show_chunk_numbers=False,
):
    fzf = fzf_bootstrap(update)

    if not fzf:
        print(FZF_NOT_FOUND)
        return 1

    push = not stage_changes and not dry_run
    check_working_directory(push)
    tg = generate_tasks(
        parameters, full=full, disable_target_task_filter=disable_target_task_filter
    )
    all_tasks = tg.tasks

    # graph_Cache created by generate_tasks, recreate the path to that file.
    cache_dir = os.path.join(
        get_state_dir(specific_to_topsrcdir=True), "cache", "taskgraph"
    )
    if full:
        graph_cache = os.path.join(cache_dir, "full_task_graph")
        dep_cache = os.path.join(cache_dir, "full_task_dependencies")
        target_set = os.path.join(cache_dir, "full_task_set")
    else:
        graph_cache = os.path.join(cache_dir, "target_task_graph")
        dep_cache = os.path.join(cache_dir, "target_task_dependencies")
        target_set = os.path.join(cache_dir, "target_task_set")

    if show_estimates:
        download_task_history_data(cache_dir=cache_dir)
        make_trimmed_taskgraph_cache(graph_cache, dep_cache, target_file=target_set)

    if not full and not disable_target_task_filter:
        all_tasks = {
            task_name: task
            for task_name, task in all_tasks.items()
            if filter_by_uncommon_try_tasks(task_name)
        }

    if test_paths:
        all_tasks = filter_tasks_by_paths(all_tasks, test_paths)
        if not all_tasks:
            return 1

    key_shortcuts = [k + ":" + v for k, v in fzf_shortcuts.items()]
    base_cmd = [
        fzf,
        "-m",
        "--bind",
        ",".join(key_shortcuts),
        "--header",
        format_header(),
        "--preview-window=right:30%",
        "--print-query",
    ]

    if show_estimates:
        base_cmd.extend(
            [
                "--preview",
                '{} {} -g {} -s -c {} -t "{{+f}}"'.format(
                    str(PurePath(sys.executable)), PREVIEW_SCRIPT, dep_cache, cache_dir
                ),
            ]
        )
    else:
        base_cmd.extend(
            [
                "--preview",
                '{} {} -t "{{+f}}"'.format(
                    str(PurePath(sys.executable)), PREVIEW_SCRIPT
                ),
            ]
        )

    if exact:
        base_cmd.append("--exact")

    selected = set()
    queries = []

    def get_tasks(query_arg=None, candidate_tasks=all_tasks):
        cmd = base_cmd[:]
        if query_arg and query_arg != "INTERACTIVE":
            cmd.extend(["-f", query_arg])

        if not show_chunk_numbers:
            fzf_tasks = set(task.chunk_pattern for task in candidate_tasks.values())
        else:
            fzf_tasks = set(candidate_tasks.keys())

        query_str, tasks = run_fzf(cmd, sorted(fzf_tasks))
        queries.append(query_str)
        return set(tasks)

    for q in query or []:
        selected |= get_tasks(q)

    for q in intersect_query or []:
        if not selected:
            selected |= get_tasks(q)
        else:
            selected &= get_tasks(
                q,
                {
                    task_name: task
                    for task_name, task in all_tasks.items()
                    if task_name in selected or task.chunk_pattern in selected
                },
            )

    if not queries:
        selected = get_tasks()

    if not selected:
        print("no tasks selected")
        return

    if save_query:
        return queries

    if not show_chunk_numbers:
        selected = set(
            task_name
            for task_name, task in all_tasks.items()
            if task.chunk_pattern in selected
        )

    # build commit message
    msg = "Fuzzy"
    args = ["query={}".format(q) for q in queries]
    if test_paths:
        args.append("paths={}".format(":".join(test_paths)))
    if args:
        msg = "{} {}".format(msg, "&".join(args))
    return push_to_try(
        "fuzzy",
        message.format(msg=msg),
        try_task_config=generate_try_task_config("fuzzy", selected, try_config),
        stage_changes=stage_changes,
        dry_run=dry_run,
        closed_tree=closed_tree,
        push_to_lando=push_to_lando,
    )
