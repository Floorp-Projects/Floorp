# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import logging
import os

import taskcluster_urls as liburls
from taskcluster import Hooks
from taskgraph.util import taskcluster as tc_util
from taskgraph.util.taskcluster import (
    _do_request,
    get_index_url,
    get_root_url,
    get_task_definition,
    get_task_url,
)

logger = logging.getLogger(__name__)


def insert_index(index_path, task_id, data=None, use_proxy=False):
    index_url = get_index_url(index_path, use_proxy=use_proxy)

    # Find task expiry.
    expires = get_task_definition(task_id, use_proxy=use_proxy)["expires"]

    response = _do_request(
        index_url,
        method="put",
        json={
            "taskId": task_id,
            "rank": 0,
            "data": data or {},
            "expires": expires,
        },
    )
    return response


def status_task(task_id, use_proxy=False):
    """Gets the status of a task given a task_id.

    In testing mode, just logs that it would have retrieved status.

    Args:
        task_id (str): A task id.
        use_proxy (bool): Whether to use taskcluster-proxy (default: False)

    Returns:
        dict: A dictionary object as defined here:
          https://docs.taskcluster.net/docs/reference/platform/queue/api#status
    """
    if tc_util.testing:
        logger.info(f"Would have gotten status for {task_id}.")
    else:
        resp = _do_request(get_task_url(task_id, use_proxy) + "/status")
        status = resp.json().get("status", {})
        return status


def state_task(task_id, use_proxy=False):
    """Gets the state of a task given a task_id.

    In testing mode, just logs that it would have retrieved state. This is a subset of the
    data returned by :func:`status_task`.

    Args:
        task_id (str): A task id.
        use_proxy (bool): Whether to use taskcluster-proxy (default: False)

    Returns:
        str: The state of the task, one of
          ``pending, running, completed, failed, exception, unknown``.
    """
    if tc_util.testing:
        logger.info(f"Would have gotten state for {task_id}.")
    else:
        status = status_task(task_id, use_proxy=use_proxy).get("state") or "unknown"
        return status


def trigger_hook(hook_group_id, hook_id, hook_payload):
    hooks = Hooks({"rootUrl": get_root_url(True)})
    response = hooks.triggerHook(hook_group_id, hook_id, hook_payload)

    logger.info(
        "Task seen here: {}/tasks/{}".format(
            get_root_url(os.environ.get("TASKCLUSTER_PROXY_URL")),
            response["status"]["taskId"],
        )
    )


def list_task_group_tasks(task_group_id):
    """Generate the tasks in a task group"""
    params = {}
    while True:
        url = liburls.api(
            get_root_url(False),
            "queue",
            "v1",
            f"task-group/{task_group_id}/list",
        )
        resp = _do_request(url, method="get", params=params).json()
        yield from resp["tasks"]
        if resp.get("continuationToken"):
            params = {"continuationToken": resp.get("continuationToken")}
        else:
            break


def list_task_group_incomplete_task_ids(task_group_id):
    states = ("running", "pending", "unscheduled")
    for task in [t["status"] for t in list_task_group_tasks(task_group_id)]:
        if task["state"] in states:
            yield task["taskId"]


def list_task_group_complete_tasks(task_group_id):
    tasks = {}
    for task in list_task_group_tasks(task_group_id):
        if task.get("status", {}).get("state", "") == "completed":
            tasks[task.get("task", {}).get("metadata", {}).get("name", "")] = task.get(
                "status", {}
            ).get("taskId", "")
    return tasks
