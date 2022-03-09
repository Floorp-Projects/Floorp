# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging

from taskgraph.util.taskcluster import get_task_definition, purge_cache

from .registry import register_callback_action

logger = logging.getLogger(__name__)


@register_callback_action(
    title="Purge Worker Caches",
    name="purge-cache",
    symbol="purge-cache",
    description=(
        "Purge any caches associated with this task "
        "across all workers of the same workertype as the task."
    ),
    order=450,
    context=[{"worker-implementation": "docker-worker"}],
)
def purge_caches_action(parameters, graph_config, input, task_group_id, task_id):
    task = get_task_definition(task_id)
    if task["payload"].get("cache"):
        for cache in task["payload"]["cache"]:
            purge_cache(
                task["provisionerId"], task["workerType"], cache, use_proxy=True
            )
    else:
        logger.info("Task has no caches. Will not clear anything!")
