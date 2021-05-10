# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import logging
import re
import sys
from functools import partial


from taskgraph.util.taskcluster import get_task_definition
from .registry import register_callback_action
from .util import (
    combine_task_graph_files,
    create_tasks,
    fetch_graph_and_labels,
    get_decision_task_id,
    get_pushes_from_params_input,
    trigger_action,
    get_tasks_with_downstream,
    rename_browsertime_vismet_task,
)

logger = logging.getLogger(__name__)
SYMBOL_REGEX = re.compile("^(.*)-[a-z0-9]{11}-bk$")
GROUP_SYMBOL_REGEX = re.compile("^(.*)-bk$")


def input_for_support_action(revision, task, times=1):
    """Generate input for action to be scheduled.

    Define what label to schedule with 'label'.
    If it is a test task that uses explicit manifests add that information.
    """
    input = {
        "label": task["metadata"]["name"],
        "revision": revision,
        "times": times,
        # We want the backfilled tasks to share the same symbol as the originating task
        "symbol": task["extra"]["treeherder"]["symbol"],
    }

    # Support tasks that are using manifest based scheduling
    if task["payload"]["env"].get("MOZHARNESS_TEST_PATHS"):
        input["test_manifests"] = json.loads(
            task["payload"]["env"]["MOZHARNESS_TEST_PATHS"]
        )

    return input


@register_callback_action(
    title="Backfill",
    name="backfill",
    permission="backfill",
    symbol="Bk",
    description=("Given a task schedule it on previous pushes in the same project."),
    order=200,
    context=[{}],  # This will be available for all tasks
    schema={
        "type": "object",
        "properties": {
            "depth": {
                "type": "integer",
                "default": 19,
                "minimum": 1,
                "maximum": 25,
                "title": "Depth",
                "description": (
                    "The number of previous pushes before the current "
                    "push to attempt to trigger this task on."
                ),
            },
            "inclusive": {
                "type": "boolean",
                "default": False,
                "title": "Inclusive Range",
                "description": (
                    "If true, the backfill will also retrigger the task "
                    "on the selected push."
                ),
            },
            "times": {
                "type": "integer",
                "default": 1,
                "minimum": 1,
                "maximum": 10,
                "title": "Times",
                "description": (
                    "The number of times to execute each job you are backfilling."
                ),
            },
        },
        "additionalProperties": False,
    },
    available=lambda parameters: True,
)
def backfill_action(parameters, graph_config, input, task_group_id, task_id):
    """
    This action takes a task ID and schedules it on previous pushes (via support action).

    To execute this action locally follow the documentation here:
    https://firefox-source-docs.mozilla.org/taskcluster/actions.html#testing-the-action-locally
    """
    task = get_task_definition(task_id)
    pushes = get_pushes_from_params_input(parameters, input)
    failed = False
    input_for_action = input_for_support_action(
        revision=parameters["head_rev"],
        task=task,
        times=input.get("times", 1),
    )

    for push_id in pushes:
        try:
            # The Gecko decision task can sometimes fail on a push and we need to handle
            # the exception that this call will produce
            push_decision_task_id = get_decision_task_id(parameters["project"], push_id)
        except Exception:
            logger.warning("Could not find decision task for push {}".format(push_id))
            # The decision task may have failed, this is common enough that we
            # don't want to report an error for it.
            continue

        try:
            trigger_action(
                action_name="backfill-task",
                # This lets the action know on which push we want to add a new task
                decision_task_id=push_decision_task_id,
                input=input_for_action,
            )
        except Exception:
            logger.exception("Failed to trigger action for {}".format(push_id))
            failed = True

    if failed:
        sys.exit(1)


def add_backfill_suffix(regex, symbol, suffix):
    m = regex.match(symbol)
    if m is None:
        symbol += suffix
    return symbol


def test_manifests_modifier(task, label, symbol, revision, test_manifests):
    """In the case of test tasks we can modify the test paths they execute."""
    if task.label != label:
        return task

    logger.debug("Modifying test_manifests for {}".format(task.label))
    test_manifests = test_manifests
    task.attributes["test_manifests"] = test_manifests
    task.task["payload"]["env"]["MOZHARNESS_TEST_PATHS"] = json.dumps(test_manifests)
    # The name/label might have been modify in new_label, thus, change it here as well
    task.task["metadata"]["name"] = task.label
    th_info = task.task["extra"]["treeherder"]
    # Use a job symbol of the originating task as defined in the backfill action
    th_info["symbol"] = add_backfill_suffix(
        SYMBOL_REGEX, th_info["symbol"], "-{}-bk".format(revision[0:11])
    )
    if th_info.get("groupSymbol"):
        # Group all backfilled tasks together
        th_info["groupSymbol"] = add_backfill_suffix(
            GROUP_SYMBOL_REGEX, th_info["groupSymbol"], "-bk"
        )
    task.task["tags"]["action"] = "backfill-task"
    return task


def do_not_modify(task):
    return task


def new_label(label, tasks):
    """This is to handle the case when a previous push does not contain a specific task label
    and we try to find a label we can reuse.

    For instance, we try to backfill chunk #3, however, a previous push does not contain such
    chunk, thus, we try to reuse another task/label.
    """
    begining_label, ending = label.rsplit("-", 1)
    if ending.isdigit():
        # We assume that the taskgraph has chunk #1 OR unnumbered chunk and we hijack it
        if begining_label in tasks:
            return begining_label
        elif begining_label + "-1" in tasks:
            return begining_label + "-1"
        else:
            raise Exception(
                "New label ({}) was not found in the task-graph".format(label)
            )
    else:
        raise Exception("{} was not found in the task-graph".format(label))


@register_callback_action(
    name="backfill-task",
    title="Backfill task on a push.",
    permission="backfill",
    symbol="backfill-task",
    description="This action is normally scheduled by the backfill action. "
    "The intent is to schedule a task on previous pushes.",
    order=500,
    context=[],
    schema={
        "type": "object",
        "properties": {
            "label": {"type": "string", "description": "A task label"},
            "revision": {
                "type": "string",
                "description": "Revision of the original push from where we backfill.",
            },
            "symbol": {
                "type": "string",
                "description": "Symbol to be used by the scheduled task.",
            },
            "test_manifests": {
                "type": "array",
                "default": [],
                "description": "An array of test manifest paths",
                "items": {"type": "string"},
            },
            "times": {
                "type": "integer",
                "default": 1,
                "minimum": 1,
                "maximum": 10,
                "title": "Times",
                "description": (
                    "The number of times to execute each job " "you are backfilling."
                ),
            },
        },
    },
)
def add_task_with_original_manifests(
    parameters, graph_config, input, task_group_id, task_id
):
    """
    This action is normally scheduled by the backfill action. The intent is to schedule a test
    task with the test manifests from the original task (if available).

    The push in which we want to schedule a new task is defined by the parameters object.

    To execute this action locally follow the documentation here:
    https://firefox-source-docs.mozilla.org/taskcluster/actions.html#testing-the-action-locally
    """
    # This step takes a lot of time when executed locally
    logger.info("Retreving the full task graph and labels.")
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )

    label = input.get("label")
    if label not in full_task_graph.tasks:
        label = new_label(label, full_task_graph.tasks)

    to_run = [label]
    if "browsertime" in label:
        if "vismet" in label:
            label = rename_browsertime_vismet_task(label)
        to_run = get_tasks_with_downstream([label], full_task_graph, label_to_taskid)

    modifier = do_not_modify
    test_manifests = input.get("test_manifests")
    # If the original task has defined test paths
    if test_manifests:
        modifier = partial(
            test_manifests_modifier,
            label=label,
            revision=input.get("revision"),
            symbol=input.get("symbol"),
            test_manifests=test_manifests,
        )

    logger.info("Creating tasks...")
    times = input.get("times", 1)
    for i in range(times):
        create_tasks(
            graph_config,
            to_run,
            full_task_graph,
            label_to_taskid,
            parameters,
            decision_task_id,
            suffix=i,
            modifier=modifier,
        )

    combine_task_graph_files(list(range(times)))
