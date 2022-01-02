# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.actions.registry import register_callback_action
from taskgraph.actions.util import (
    combine_task_graph_files,
    create_tasks,
    fetch_graph_and_labels,
)


@register_callback_action(
    name="add-new-jobs",
    title="Add new jobs",
    generic=True,
    symbol="add-new",
    description="Add new jobs using task labels.",
    order=100,
    context=[],
    schema={
        "type": "object",
        "properties": {
            "tasks": {
                "type": "array",
                "description": "An array of task labels",
                "items": {"type": "string"},
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
def add_new_jobs_action(parameters, graph_config, input, task_group_id, task_id):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )

    to_run = []
    for elem in input["tasks"]:
        if elem in full_task_graph.tasks:
            to_run.append(elem)
        else:
            raise Exception(f"{elem} was not found in the task-graph")

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
    combine_task_graph_files(list(range(times)))
