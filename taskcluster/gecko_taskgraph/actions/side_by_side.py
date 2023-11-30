# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
import sys
from functools import partial

from taskgraph.util.taskcluster import get_artifact, get_task_definition

from ..util.taskcluster import list_task_group_complete_tasks
from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels, get_decision_task_id, get_pushes

logger = logging.getLogger(__name__)


def input_for_support_action(revision, base_revision, base_branch, task):
    """Generate input for action to be scheduled.

    Define what label to schedule with 'label'.
    If it is a test task that uses explicit manifests add that information.
    """
    platform, test_name = task["metadata"]["name"].split("/opt-")
    new_branch = os.environ.get("GECKO_HEAD_REPOSITORY", "/try").split("/")[-1]
    symbol = task["extra"]["treeherder"]["symbol"]
    input = {
        "label": "perftest-linux-side-by-side",
        "symbol": symbol,
        "new_revision": revision,
        "base_revision": base_revision,
        "test_name": test_name,
        "platform": platform,
        "base_branch": base_branch,
        "new_branch": new_branch,
    }

    return input


def side_by_side_modifier(task, input):
    if task.label != input["label"]:
        return task

    # Make side-by-side job searchable by the platform, test name, and revisions
    # it was triggered for
    task.task["metadata"][
        "name"
    ] = f"{input['platform']} {input['test_name']} {input['base_revision'][:12]} {input['new_revision'][:12]}"
    # Use a job symbol to include the symbol of the job the side-by-side
    # is running for
    task.task["extra"]["treeherder"]["symbol"] += f"-{input['symbol']}"

    cmd = task.task["payload"]["command"]
    task.task["payload"]["command"][1][-1] = cmd[1][-1].format(**input)

    return task


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
    This action does a side-by-side comparison between current revision and
    the revision entered manually or the latest revision that ran the
    pageload job (via support action).

    To execute this action locally follow the documentation here:
    https://firefox-source-docs.mozilla.org/taskcluster/actions.html#testing-the-action-locally
    """
    task = get_task_definition(task_id)
    decision_task_id, full_task_graph, label_to_taskid, _ = fetch_graph_and_labels(
        parameters, graph_config
    )
    # TODO: find another way to detect side-by-side comparable jobs
    # (potentially lookig at the visual metrics flag)
    if not (
        "browsertime-tp6" in task["metadata"]["name"]
        or "welcome" in task["metadata"]["name"]
    ):
        logger.exception(
            f"Task {task['metadata']['name']} is not side-by-side comparable."
        )
        return

    failed = False
    input_for_action = {}

    if input.get("revision"):
        # If base_revision was introduced manually, use that
        input_for_action = input_for_support_action(
            revision=parameters["head_rev"],
            base_revision=input.get("revision"),
            base_branch=input.get("project"),
            task=task,
        )
    else:
        current_push_id = int(parameters["pushlog_id"]) - 1
        # Go decrementally through pushlog_id, get push data, decision task id,
        # full task graph and everything needed to find which of the past revisions
        # ran the pageload job to compare against
        while int(parameters["pushlog_id"]) - current_push_id < 30:
            pushes = get_pushes(
                project=parameters["head_repository"],
                end_id=current_push_id,
                depth=1,
                full_response=True,
            )
            try:
                # Get label-to-taskid.json artifact + the tasks triggered
                # by the action tasks at a later time than the decision task
                current_decision_task_id = get_decision_task_id(
                    parameters["project"], current_push_id
                )
                current_task_group_id = get_task_definition(current_decision_task_id)[
                    "taskGroupId"
                ]
                current_label_to_taskid = get_artifact(
                    current_decision_task_id, "public/label-to-taskid.json"
                )
                current_full_label_to_taskid = current_label_to_taskid.copy()
                action_task_triggered = list_task_group_complete_tasks(
                    current_task_group_id
                )
                current_full_label_to_taskid.update(action_task_triggered)
                if task["metadata"]["name"] in current_full_label_to_taskid.keys():
                    input_for_action = input_for_support_action(
                        revision=parameters["head_rev"],
                        base_revision=pushes[str(current_push_id)]["changesets"][-1],
                        base_branch=input.get("project", parameters["project"]),
                        task=task,
                    )
                    break
            except Exception:
                logger.warning(
                    f"Could not find decision task for push {current_push_id}"
                )
                # The decision task may have failed, this is common enough that we
                # don't want to report an error for it.
                continue
            current_push_id -= 1
        if not input_for_action:
            raise Exception(
                "Could not find a side-by-side comparable task within a depth of 30 revisions."
            )

    try:
        create_tasks(
            graph_config,
            [input_for_action["label"]],
            full_task_graph,
            label_to_taskid,
            parameters,
            decision_task_id,
            modifier=partial(side_by_side_modifier, input=input_for_action),
        )
    except Exception as e:
        logger.exception(f"Failed to trigger action: {e}.")
        failed = True

    if failed:
        sys.exit(1)
