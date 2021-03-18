# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from .registry import register_callback_action

from .util import (
    combine_task_graph_files,
    create_tasks,
    fetch_graph_and_labels,
    get_downstream_browsertime_tasks,
    rename_browsertime_vismet_task,
)


@register_callback_action(
    name="add-new-jobs",
    title="Add new jobs",
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
    browsertime_tasks = []
    for elem in input["tasks"]:
        if elem in full_task_graph.tasks:
            if "browsertime" in elem:
                label = elem
                if "vismet" in label:
                    label = rename_browsertime_vismet_task(label)
                browsertime_tasks.append(label)
            else:
                to_run.append(elem)
        else:
            raise Exception("{} was not found in the task-graph".format(elem))
    if len(browsertime_tasks) > 0:
        to_run.extend(
            list(
                get_downstream_browsertime_tasks(
                    browsertime_tasks, full_task_graph, label_to_taskid
                )
            )
        )

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
