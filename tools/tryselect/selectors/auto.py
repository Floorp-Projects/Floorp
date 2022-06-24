# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.util.python_path import find_object

from ..cli import BaseTryParser
from ..push import push_to_try


TRY_AUTO_PARAMETERS = {
    "optimize_strategies": "gecko_taskgraph.optimize:tryselect.bugbug_reduced_manifests_config_selection_medium",  # noqa
    "optimize_target_tasks": True,
    "target_tasks_method": "try_auto",
    "test_manifest_loader": "bugbug",
    "try_mode": "try_auto",
    "try_task_config": {},
}


class AutoParser(BaseTryParser):
    name = "auto"
    common_groups = ["push"]
    task_configs = [
        "artifact",
        "env",
        "chemspill-prio",
        "disable-pgo",
        "worker-overrides",
    ]
    arguments = [
        [
            ["--strategy"],
            {
                "default": None,
                "help": "Override the default optimization strategy. Valid values "
                "are the experimental strategies defined at the bottom of "
                "`taskcluster/gecko_taskgraph/optimize/__init__.py`.",
            },
        ],
        [
            ["--tasks-regex"],
            {
                "default": [],
                "action": "append",
                "help": "Apply a regex filter to the tasks selected. Specifying "
                "multiple times schedules the union of computed tasks.",
            },
        ],
        [
            ["--tasks-regex-exclude"],
            {
                "default": [],
                "action": "append",
                "help": "Apply a regex filter to the tasks selected. Specifying "
                "multiple times excludes computed tasks matching any regex.",
            },
        ],
    ]

    def validate(self, args):
        super().validate(args)

        if args.strategy:
            if ":" not in args.strategy:
                args.strategy = "gecko_taskgraph.optimize:tryselect.{}".format(
                    args.strategy
                )

            try:
                obj = find_object(args.strategy)
            except (ImportError, AttributeError):
                self.error("invalid module path '{}'".format(args.strategy))

            if not isinstance(obj, dict):
                self.error("object at '{}' must be a dict".format(args.strategy))


def run(
    message="{msg}",
    stage_changes=False,
    dry_run=False,
    closed_tree=False,
    strategy=None,
    tasks_regex=None,
    tasks_regex_exclude=None,
    try_config=None,
    **ignored
):
    msg = message.format(msg="Tasks automatically selected.")

    params = TRY_AUTO_PARAMETERS.copy()
    if try_config:
        params["try_task_config"] = try_config

    if strategy:
        params["optimize_strategies"] = strategy

    if tasks_regex or tasks_regex_exclude:
        params.setdefault("try_task_config", {})["tasks-regex"] = {}
        params["try_task_config"]["tasks-regex"]["include"] = tasks_regex
        params["try_task_config"]["tasks-regex"]["exclude"] = tasks_regex_exclude

    task_config = {
        "version": 2,
        "parameters": params,
    }
    return push_to_try(
        "auto",
        msg,
        try_task_config=task_config,
        stage_changes=stage_changes,
        dry_run=dry_run,
        closed_tree=closed_tree,
    )
