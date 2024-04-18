# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os
from concurrent import futures

import requests

from taskgraph.util.taskcluster import (
    CONCURRENCY,
    cancel_task,
    list_task_group_incomplete_tasks,
)

from .registry import register_callback_action

logger = logging.getLogger(__name__)


@register_callback_action(
    title="Cancel All",
    name="cancel-all",
    generic=True,
    symbol="cAll",
    description=(
        "Cancel all running and pending tasks created by the decision task "
        "this action task is associated with."
    ),
    order=400,
    context=[],
)
def cancel_all_action(parameters, graph_config, input, task_group_id, task_id):
    def do_cancel_task(task_id):
        logger.info(f"Cancelling task {task_id}")
        try:
            cancel_task(task_id, use_proxy=True)
        except requests.HTTPError as e:
            if e.response.status_code == 409:
                # A 409 response indicates that this task is past its deadline.  It
                # cannot be cancelled at this time, but it's also not running
                # anymore, so we can ignore this error.
                logger.info(
                    "Task {} is past its deadline and cannot be cancelled.".format(
                        task_id
                    )
                )
                return
            raise

    own_task_id = os.environ.get("TASK_ID", "")
    to_cancel = [
        t for t in list_task_group_incomplete_tasks(task_group_id) if t != own_task_id
    ]
    logger.info(f"Cancelling {len(to_cancel)} tasks")
    with futures.ThreadPoolExecutor(CONCURRENCY) as e:
        cancel_futs = [e.submit(do_cancel_task, t) for t in to_cancel]
        for f in futures.as_completed(cancel_futs):
            f.result()
