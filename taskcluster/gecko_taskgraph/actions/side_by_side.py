# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import sys
from functools import partial

from taskgraph.util.taskcluster import get_task_definition

from .registry import register_callback_action
from .util import (
    create_tasks,
    fetch_graph_and_labels,
)

logger = logging.getLogger(__name__)


def input_for_support_action(revision, base_revision, base_branch, task):
    """Generate input for action to be scheduled.

    Define what label to schedule with 'label'.
    If it is a test task that uses explicit manifests add that information.
    """
    platform, test_name = task["metadata"]["name"].split("/opt-")
    new_branch = os.environ.get("GECKO_HEAD_REPOSITORY", "/try").split("/")[-1]
    input = {
        "label": "perftest-linux-side-by-side",
        "new_revision": revision,
        "base_revision": base_revision,
        "test_name": test_name,
        "platform": platform,
        "base_branch": base_branch,
        "new_branch": new_branch,
    }

    return input


@register_callback_action(
    title="Side by side",
    name="side-by-side",
    symbol="gen-sxs",
    description=(
        "Given a performance test pageload job generate a side-by-side comparison against"
        "the pageload job from the revision at the input."
    ),
    order=200,
    context=[{"test-type": "raptor"}],
    schema={
        "type": "object",
        "properties": {
            "revision": {
                "type": "string",
                "default": "",
                "description": "Revision of the push against the comparison is wanted.",
            },
            "project": {
                "type": "string",
                "default": "autoland",
                "description": "Revision of the push against the comparison is wanted.",
            },
        },
        "additionalProperties": False,
    },
)
def side_by_side_action(parameters, graph_config, input, task_group_id, task_id):
    """
    This action takes a task ID and schedules it on previous pushes (via support action).

    To execute this action locally follow the documentation here:
    https://firefox-source-docs.mozilla.org/taskcluster/actions.html#testing-the-action-locally
    """
    task = get_task_definition(task_id)
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )
    input_for_action = input_for_support_action(
        revision=parameters["head_rev"],
        base_revision=input.get("revision"),
        base_branch=input.get("project"),
        task=task,
    )

    failed = False

    def modifier(task, input):
        if task.label != input["label"]:
            return task

        cmd = task.task["payload"]["command"]
        task.task["payload"]["command"][1][-1] = cmd[1][-1].format(**input)

        return task

    try:
        create_tasks(
            graph_config,
            [input_for_action["label"]],
            full_task_graph,
            label_to_taskid,
            parameters,
            decision_task_id,
            modifier=partial(modifier, input=input_for_action),
        )
    except Exception:
        logger.exception("Failed to trigger action.")
        failed = True

    if failed:
        sys.exit(1)
