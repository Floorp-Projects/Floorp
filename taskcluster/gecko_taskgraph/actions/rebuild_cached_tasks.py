# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels


@register_callback_action(
    name="rebuild-cached-tasks",
    title="Rebuild Cached Tasks",
    symbol="rebuild-cached",
    description="Rebuild cached tasks.",
    order=1000,
    context=[],
)
def rebuild_cached_tasks_action(
    parameters, graph_config, input, task_group_id, task_id
):
    decision_task_id, full_task_graph, label_to_taskid, _ = fetch_graph_and_labels(
        parameters, graph_config
    )
    cached_tasks = [
        label
        for label, task in full_task_graph.tasks.items()
        if task.attributes.get("cached_task", False)
    ]
    if cached_tasks:
        create_tasks(
            graph_config,
            cached_tasks,
            full_task_graph,
            label_to_taskid,
            parameters,
            decision_task_id,
        )
