# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import join_symbol, split_symbol

from gecko_taskgraph.util.copy_task import copy_task

transforms = TransformSequence()


@transforms.add
def test_confirm_failure_tasks(config, tasks):
    """Copy test-* tasks to have -cf copy."""
    for task in tasks:
        if "backlog" in task["suite"] or "failure" in task["suite"]:
            yield task
            continue

        # support mochitest, xpcshell, reftest, wpt*
        if any(
            task["suite"].startswith(s)
            for s in ("mochitest", "reftest", "xpcshell", "web-platform")
        ):
            env = config.params.get("try_task_config", {}) or {}
            env = env.get("templates", {}).get("env", {})

            cftask = copy_task(task)

            # when scheduled other settings will be made
            cftask["tier"] = 3
            cftask["confirm-failure"] = True
            group, symbol = split_symbol(cftask["treeherder-symbol"])
            group += "-cf"
            cftask["treeherder-symbol"] = join_symbol(group, symbol)
            cftask["run-on-projects"] = []
            cftask["optimization"] = {"always": None}
            yield cftask

        yield task
