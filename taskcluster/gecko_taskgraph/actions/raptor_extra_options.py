# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

from taskgraph.util.taskcluster import get_task_definition

from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels

logger = logging.getLogger(__name__)


@register_callback_action(
    title="Raptor Extra Options",
    name="raptor-extra-options",
    symbol="rxo",
    description=(
        "Allows the user to rerun raptor-browsertime tasks with additional arguments."
    ),
    order=200,
    context=[{"test-type": "raptor"}],
    schema={
        "type": "object",
        "properties": {
            "extra_options": {
                "type": "string",
                "default": "",
                "description": "A space-delimited string of extra options "
                "to be passed into a raptor-browsertime test."
                "This also works with options with values, where the values "
                "should be set as an assignment e.g. browser-cycles=3 "
                "Passing multiple extra options could look something this:  "
                "`verbose browser-cycles=3` where the test runs with verbose "
                "mode on and the browser cycles only 3 times.",
            }
        },
    },
    available=lambda parameters: True,
)
def raptor_extra_options_action(
    parameters, graph_config, input, task_group_id, task_id
):
    decision_task_id, full_task_graph, label_to_taskid, _ = fetch_graph_and_labels(
        parameters, graph_config
    )
    task = get_task_definition(task_id)
    label = task["metadata"]["name"]

    def modifier(task):
        if task.label != label:
            return task

        if task.task["payload"]["env"].get("PERF_FLAGS"):
            task.task["payload"]["env"]["PERF_FLAGS"] += " " + input.get(
                "extra_options"
            )
        else:
            task.task["payload"]["env"].setdefault(
                "PERF_FLAGS", input.get("extra_options")
            )

        task.task["extra"]["treeherder"]["symbol"] += "-rxo"
        task.task["extra"]["treeherder"]["groupName"] += " (extra options run)"
        return task

    create_tasks(
        graph_config,
        [label],
        full_task_graph,
        label_to_taskid,
        parameters,
        decision_task_id,
        modifier=modifier,
    )
