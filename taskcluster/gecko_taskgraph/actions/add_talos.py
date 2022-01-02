# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging


from ..target_tasks import standard_filter
from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels

logger = logging.getLogger(__name__)


@register_callback_action(
    name="run-all-talos",
    title="Run All Talos Tests",
    symbol="raT",
    description="Add all Talos tasks to a push.",
    order=150,
    context=[],
    schema={
        "type": "object",
        "properties": {
            "times": {
                "type": "integer",
                "default": 1,
                "minimum": 1,
                "maximum": 6,
                "title": "Times",
                "description": "How many times to run each task.",
            }
        },
        "additionalProperties": False,
    },
)
def add_all_talos(parameters, graph_config, input, task_group_id, task_id):
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )

    times = input.get("times", 1)
    for i in range(times):
        to_run = [
            label
            for label, entry in full_task_graph.tasks.items()
            if "talos_try_name" in entry.attributes
            and standard_filter(entry, parameters)
        ]

        create_tasks(
            graph_config,
            to_run,
            full_task_graph,
            label_to_taskid,
            parameters,
            decision_task_id,
        )
        logger.info(f"Scheduled {len(to_run)} talos tasks (time {i + 1}/{times})")
