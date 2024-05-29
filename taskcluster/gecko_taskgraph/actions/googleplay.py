# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

from .registry import register_callback_action
from .util import create_tasks, fetch_graph_and_labels

logger = logging.getLogger(__name__)


@register_callback_action(
    name="googleplay",
    title="Submit android apps to google play",
    symbol="gp",
    description="Submit android apps to google play",
    order=150,
    context=[],
    available=lambda params: params["project"] == "mozilla-central",
    schema={"type": "object", "properties": {}},
    permission="googleplay",
)
def add_push_bundle(parameters, graph_config, input, task_group_id, task_id):
    decision_task_id, full_task_graph, label_to_taskid, _ = fetch_graph_and_labels(
        parameters, graph_config
    )

    def filter(task):
        if task.kind != "push-bundle":
            return False
        return task.attributes["build-type"] in ("fenix-nightly", "focus-nightly")

    to_run = [label for label, task in full_task_graph.tasks.items() if filter(task)]

    create_tasks(
        graph_config,
        to_run,
        full_task_graph,
        label_to_taskid,
        parameters,
        decision_task_id,
    )
    logger.info(f"Scheduled {len(to_run)} push-bundle tasks")
