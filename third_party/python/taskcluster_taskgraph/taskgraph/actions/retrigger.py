# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import sys
import textwrap

from slugid import nice as slugid

from .util import (
    combine_task_graph_files,
    create_tasks,
    fetch_graph_and_labels,
    relativize_datestamps,
    create_task_from_def,
)
from .registry import register_callback_action
from taskgraph.util import taskcluster

logger = logging.getLogger(__name__)

RERUN_STATES = ("exception", "failed")


def _should_retrigger(task_graph, label):
    """
    Return whether a given task in the taskgraph should be retriggered.

    This handles the case where the task isn't there by assuming it should not be.
    """
    if label not in task_graph:
        logger.info(
            "Task {} not in full taskgraph, assuming task should not be retriggered.".format(
                label
            )
        )
        return False
    return task_graph[label].attributes.get("retrigger", False)


@register_callback_action(
    title="Retrigger",
    name="retrigger",
    symbol="rt",
    cb_name="retrigger-decision",
    description=textwrap.dedent(
        """\
        Create a clone of the task (retriggering decision, action, and cron tasks requires
        special scopes)."""
    ),
    order=11,
    context=[
        {"kind": "decision-task"},
        {"kind": "action-callback"},
        {"kind": "cron-task"},
    ],
)
def retrigger_decision_action(parameters, graph_config, input, task_group_id, task_id):
    """For a single task, we try to just run exactly the same task once more.
    It's quite possible that we don't have the scopes to do so (especially for
    an action), but this is best-effort."""

    # make all of the timestamps relative; they will then be turned back into
    # absolute timestamps relative to the current time.
    task = taskcluster.get_task_definition(task_id)
    task = relativize_datestamps(task)
    create_task_from_def(slugid(), task, parameters["level"])


@register_callback_action(
    title="Retrigger",
    name="retrigger",
    symbol="rt",
    generic=True,
    description=("Create a clone of the task."),
    order=19,  # must be greater than other orders in this file, as this is the fallback version
    context=[{"retrigger": "true"}],
    schema={
        "type": "object",
        "properties": {
            "downstream": {
                "type": "boolean",
                "description": (
                    "If true, downstream tasks from this one will be cloned as well. "
                    "The dependencies will be updated to work with the new task at the root."
                ),
                "default": False,
            },
            "times": {
                "type": "integer",
                "default": 1,
                "minimum": 1,
                "maximum": 100,
                "title": "Times",
                "description": "How many times to run each task.",
            },
        },
    },
)
@register_callback_action(
    title="Retrigger (disabled)",
    name="retrigger",
    cb_name="retrigger-disabled",
    symbol="rt",
    generic=True,
    description=(
        "Create a clone of the task.\n\n"
        "This type of task should typically be re-run instead of re-triggered."
    ),
    order=20,  # must be greater than other orders in this file, as this is the fallback version
    context=[{}],
    schema={
        "type": "object",
        "properties": {
            "downstream": {
                "type": "boolean",
                "description": (
                    "If true, downstream tasks from this one will be cloned as well. "
                    "The dependencies will be updated to work with the new task at the root."
                ),
                "default": False,
            },
            "times": {
                "type": "integer",
                "default": 1,
                "minimum": 1,
                "maximum": 100,
                "title": "Times",
                "description": "How many times to run each task.",
            },
            "force": {
                "type": "boolean",
                "default": False,
                "description": (
                    "This task should not be re-triggered. "
                    "This can be overridden by passing `true` here."
                ),
            },
        },
    },
)
def retrigger_action(parameters, graph_config, input, task_group_id, task_id):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )

    task = taskcluster.get_task_definition(task_id)
    label = task["metadata"]["name"]

    with_downstream = " "
    to_run = [label]

    if not input.get("force", None) and not _should_retrigger(full_task_graph, label):
        logger.info(
            "Not retriggering task {}, task should not be retrigged "
            "and force not specified.".format(label)
        )
        sys.exit(1)

    if input.get("downstream"):
        to_run = full_task_graph.graph.transitive_closure(
            set(to_run), reverse=True
        ).nodes
        to_run = to_run & set(label_to_taskid.keys())
        with_downstream = " (with downstream) "

    times = input.get("times", 1)
    for i in range(times):
        create_tasks(
            graph_config,
            to_run,
            full_task_graph,
            label_to_taskid,
            parameters,
            decision_task_id,
            i,
        )

        logger.info(f"Scheduled {label}{with_downstream}(time {i + 1}/{times})")
    combine_task_graph_files(list(range(times)))


@register_callback_action(
    title="Rerun",
    name="rerun",
    generic=True,
    symbol="rr",
    description=(
        "Rerun a task.\n\n"
        "This only works on failed or exception tasks in the original taskgraph,"
        " and is CoT friendly."
    ),
    order=300,
    context=[{}],
    schema={"type": "object", "properties": {}},
)
def rerun_action(parameters, graph_config, input, task_group_id, task_id):
    task = taskcluster.get_task_definition(task_id)
    parameters = dict(parameters)
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )
    label = task["metadata"]["name"]
    if task_id not in label_to_taskid.values():
        logger.error(
            "Refusing to rerun {}: taskId {} not in decision task {} label_to_taskid!".format(
                label, task_id, decision_task_id
            )
        )

    _rerun_task(task_id, label)


def _rerun_task(task_id, label):
    status = taskcluster.status_task(task_id)
    if status not in RERUN_STATES:
        logger.warning(
            "No need to rerun {}: state '{}' not in {}!".format(
                label, status, RERUN_STATES
            )
        )
        return
    taskcluster.rerun_task(task_id)
    logger.info(f"Reran {label}")


@register_callback_action(
    title="Retrigger",
    name="retrigger-multiple",
    symbol="rt",
    generic=True,
    description=("Create a clone of the task."),
    context=[],
    schema={
        "type": "object",
        "properties": {
            "requests": {
                "type": "array",
                "items": {
                    "tasks": {
                        "type": "array",
                        "description": "An array of task labels",
                        "items": {"type": "string"},
                    },
                    "times": {
                        "type": "integer",
                        "minimum": 1,
                        "maximum": 100,
                        "title": "Times",
                        "description": "How many times to run each task.",
                    },
                    "additionalProperties": False,
                },
            },
            "additionalProperties": False,
        },
    },
)
def retrigger_multiple(parameters, graph_config, input, task_group_id, task_id):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )

    suffixes = []
    for i, request in enumerate(input.get("requests", [])):
        times = request.get("times", 1)
        rerun_tasks = [
            label
            for label in request.get("tasks")
            if not _should_retrigger(full_task_graph, label)
        ]
        retrigger_tasks = [
            label
            for label in request.get("tasks")
            if _should_retrigger(full_task_graph, label)
        ]

        for label in rerun_tasks:
            # XXX we should not re-run tasks pulled in from other pushes
            # In practice, this shouldn't matter, as only completed tasks
            # are pulled in from other pushes and treeherder won't pass
            # those labels.
            _rerun_task(label_to_taskid[label], label)

        for j in range(times):
            suffix = f"{i}-{j}"
            suffixes.append(suffix)
            create_tasks(
                graph_config,
                retrigger_tasks,
                full_task_graph,
                label_to_taskid,
                parameters,
                decision_task_id,
                suffix,
            )

    combine_task_graph_files(suffixes)
