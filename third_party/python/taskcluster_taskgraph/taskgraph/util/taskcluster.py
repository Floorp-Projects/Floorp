# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import copy
import datetime
import functools
import logging
import os
from typing import Dict, List, Union

import requests
import taskcluster_urls as liburls
from requests.packages.urllib3.util.retry import Retry

from taskgraph.task import Task
from taskgraph.util import yaml

logger = logging.getLogger(__name__)

# this is set to true for `mach taskgraph action-callback --test`
testing = False

# Default rootUrl to use if none is given in the environment; this should point
# to the production Taskcluster deployment used for CI.
PRODUCTION_TASKCLUSTER_ROOT_URL = None

# the maximum number of parallel Taskcluster API calls to make
CONCURRENCY = 50


@functools.lru_cache(maxsize=None)
def get_root_url(use_proxy):
    """Get the current TASKCLUSTER_ROOT_URL.

    When running in a task, this must come from $TASKCLUSTER_ROOT_URL; when run
    on the command line, a default may be provided that points to the
    production deployment of Taskcluster. If use_proxy is set, this attempts to
    get TASKCLUSTER_PROXY_URL instead, failing if it is not set.
    """
    if use_proxy:
        try:
            return liburls.normalize_root_url(os.environ["TASKCLUSTER_PROXY_URL"])
        except KeyError:
            if "TASK_ID" not in os.environ:
                raise RuntimeError(
                    "taskcluster-proxy is not available when not executing in a task"
                )
            else:
                raise RuntimeError("taskcluster-proxy is not enabled for this task")

    if "TASKCLUSTER_ROOT_URL" in os.environ:
        logger.debug(
            "Running in Taskcluster instance {}{}".format(
                os.environ["TASKCLUSTER_ROOT_URL"],
                (
                    " with taskcluster-proxy"
                    if "TASKCLUSTER_PROXY_URL" in os.environ
                    else ""
                ),
            )
        )
        return liburls.normalize_root_url(os.environ["TASKCLUSTER_ROOT_URL"])

    if "TASK_ID" in os.environ:
        raise RuntimeError("$TASKCLUSTER_ROOT_URL must be set when running in a task")

    if PRODUCTION_TASKCLUSTER_ROOT_URL is None:
        raise RuntimeError(
            "Could not detect Taskcluster instance, set $TASKCLUSTER_ROOT_URL"
        )

    logger.debug("Using default TASKCLUSTER_ROOT_URL")
    return liburls.normalize_root_url(PRODUCTION_TASKCLUSTER_ROOT_URL)


def requests_retry_session(
    retries,
    backoff_factor=0.1,
    status_forcelist=(500, 502, 503, 504),
    concurrency=CONCURRENCY,
    session=None,
):
    session = session or requests.Session()
    retry = Retry(
        total=retries,
        read=retries,
        connect=retries,
        backoff_factor=backoff_factor,
        status_forcelist=status_forcelist,
    )

    # Default HTTPAdapter uses 10 connections. Mount custom adapter to increase
    # that limit. Connections are established as needed, so using a large value
    # should not negatively impact performance.
    http_adapter = requests.adapters.HTTPAdapter(
        pool_connections=concurrency,
        pool_maxsize=concurrency,
        max_retries=retry,
    )
    session.mount("http://", http_adapter)
    session.mount("https://", http_adapter)

    return session


@functools.lru_cache(maxsize=None)
def get_session():
    return requests_retry_session(retries=5)


def _do_request(url, method=None, **kwargs):
    if method is None:
        method = "post" if kwargs else "get"

    session = get_session()
    if method == "get":
        kwargs["stream"] = True

    response = getattr(session, method)(url, **kwargs)

    if response.status_code >= 400:
        # Consume content before raise_for_status, so that the connection can be
        # reused.
        response.content
    response.raise_for_status()
    return response


def _handle_artifact(path, response):
    if path.endswith(".json"):
        return response.json()
    if path.endswith(".yml"):
        return yaml.load_stream(response.text)
    response.raw.read = functools.partial(response.raw.read, decode_content=True)
    return response.raw


def get_artifact_url(task_id, path, use_proxy=False):
    artifact_tmpl = liburls.api(
        get_root_url(use_proxy), "queue", "v1", "task/{}/artifacts/{}"
    )
    return artifact_tmpl.format(task_id, path)


def get_artifact(task_id, path, use_proxy=False):
    """
    Returns the artifact with the given path for the given task id.

    If the path ends with ".json" or ".yml", the content is deserialized as,
    respectively, json or yaml, and the corresponding python data (usually
    dict) is returned.
    For other types of content, a file-like object is returned.
    """
    response = _do_request(get_artifact_url(task_id, path, use_proxy))
    return _handle_artifact(path, response)


def list_artifacts(task_id, use_proxy=False):
    response = _do_request(get_artifact_url(task_id, "", use_proxy).rstrip("/"))
    return response.json()["artifacts"]


def get_artifact_prefix(task):
    prefix = None
    if isinstance(task, dict):
        prefix = task.get("attributes", {}).get("artifact_prefix")
    elif isinstance(task, Task):
        prefix = task.attributes.get("artifact_prefix")
    else:
        raise Exception(f"Can't find artifact-prefix of non-task: {task}")
    return prefix or "public/build"


def get_artifact_path(task, path):
    return f"{get_artifact_prefix(task)}/{path}"


def get_index_url(index_path, use_proxy=False, multiple=False):
    index_tmpl = liburls.api(get_root_url(use_proxy), "index", "v1", "task{}/{}")
    return index_tmpl.format("s" if multiple else "", index_path)


def find_task_id(index_path, use_proxy=False):
    try:
        response = _do_request(get_index_url(index_path, use_proxy))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 404:
            raise KeyError(f"index path {index_path} not found")
        raise
    return response.json()["taskId"]


def find_task_id_batched(index_paths, use_proxy=False):
    """Gets the task id of multiple tasks given their respective index.

    Args:
        index_paths (List[str]): A list of task indexes.
        use_proxy (bool): Whether to use taskcluster-proxy (default: False)

    Returns:
        Dict[str, str]: A dictionary object mapping each valid index path
                        to its respective task id.

    See the endpoint here:
        https://docs.taskcluster.net/docs/reference/core/index/api#findTasksAtIndex
    """
    endpoint = liburls.api(get_root_url(use_proxy), "index", "v1", "tasks/indexes")
    task_ids = {}
    continuation_token = None

    while True:
        response = _do_request(
            endpoint,
            json={
                "indexes": index_paths,
            },
            params={"continuationToken": continuation_token},
        )

        response_data = response.json()
        if not response_data["tasks"]:
            break
        response_tasks = response_data["tasks"]
        if (len(task_ids) + len(response_tasks)) > len(index_paths):
            # Sanity check
            raise ValueError("more task ids were returned than were asked for")
        task_ids.update((t["namespace"], t["taskId"]) for t in response_tasks)

        continuationToken = response_data.get("continuationToken")
        if continuationToken is None:
            break
    return task_ids


def get_artifact_from_index(index_path, artifact_path, use_proxy=False):
    full_path = index_path + "/artifacts/" + artifact_path
    response = _do_request(get_index_url(full_path, use_proxy))
    return _handle_artifact(full_path, response)


def list_tasks(index_path, use_proxy=False):
    """
    Returns a list of task_ids where each task_id is indexed under a path
    in the index. Results are sorted by expiration date from oldest to newest.
    """
    results = []
    data = {}
    while True:
        response = _do_request(
            get_index_url(index_path, use_proxy, multiple=True), json=data
        )
        response = response.json()
        results += response["tasks"]
        if response.get("continuationToken"):
            data = {"continuationToken": response.get("continuationToken")}
        else:
            break

    # We can sort on expires because in the general case
    # all of these tasks should be created with the same expires time so they end up in
    # order from earliest to latest action. If more correctness is needed, consider
    # fetching each task and sorting on the created date.
    results.sort(key=lambda t: parse_time(t["expires"]))
    return [t["taskId"] for t in results]


def parse_time(timestamp):
    """Turn a "JSON timestamp" as used in TC APIs into a datetime"""
    return datetime.datetime.strptime(timestamp, "%Y-%m-%dT%H:%M:%S.%fZ")


def get_task_url(task_id, use_proxy=False):
    task_tmpl = liburls.api(get_root_url(use_proxy), "queue", "v1", "task/{}")
    return task_tmpl.format(task_id)


@functools.lru_cache(maxsize=None)
def get_task_definition(task_id, use_proxy=False):
    response = _do_request(get_task_url(task_id, use_proxy))
    return response.json()


def cancel_task(task_id, use_proxy=False):
    """Cancels a task given a task_id. In testing mode, just logs that it would
    have cancelled."""
    if testing:
        logger.info(f"Would have cancelled {task_id}.")
    else:
        _do_request(get_task_url(task_id, use_proxy) + "/cancel", json={})


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
    if testing:
        logger.info(f"Would have gotten status for {task_id}.")
    else:
        resp = _do_request(get_task_url(task_id, use_proxy) + "/status")
        status = resp.json().get("status", {})
        return status


def status_task_batched(task_ids, use_proxy=False):
    """Gets the status of multiple tasks given task_ids.

    In testing mode, just logs that it would have retrieved statuses.

    Args:
        task_id (List[str]): A list of task ids.
        use_proxy (bool): Whether to use taskcluster-proxy (default: False)

    Returns:
        dict: A dictionary object as defined here:
          https://docs.taskcluster.net/docs/reference/platform/queue/api#statuses
    """
    if testing:
        logger.info(f"Would have gotten status for {len(task_ids)} tasks.")
        return
    endpoint = liburls.api(get_root_url(use_proxy), "queue", "v1", "tasks/status")
    statuses = {}
    continuation_token = None

    while True:
        response = _do_request(
            endpoint,
            json={
                "taskIds": task_ids,
            },
            params={
                "continuationToken": continuation_token,
            },
        )
        response_data = response.json()
        if not response_data["statuses"]:
            break
        response_tasks = response_data["statuses"]
        if (len(statuses) + len(response_tasks)) > len(task_ids):
            raise ValueError("more task statuses were returned than were asked for")
        statuses.update((t["taskId"], t["status"]) for t in response_tasks)
        continuationToken = response_data.get("continuationToken")
        if continuationToken is None:
            break
    return statuses


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
    if testing:
        logger.info(f"Would have gotten state for {task_id}.")
    else:
        status = status_task(task_id, use_proxy=use_proxy).get("state") or "unknown"
        return status


def rerun_task(task_id):
    """Reruns a task given a task_id. In testing mode, just logs that it would
    have reran."""
    if testing:
        logger.info(f"Would have rerun {task_id}.")
    else:
        _do_request(get_task_url(task_id, use_proxy=True) + "/rerun", json={})


def get_current_scopes():
    """Get the current scopes.  This only makes sense in a task with the Taskcluster
    proxy enabled, where it returns the actual scopes accorded to the task."""
    auth_url = liburls.api(get_root_url(True), "auth", "v1", "scopes/current")
    resp = _do_request(auth_url)
    return resp.json().get("scopes", [])


def get_purge_cache_url(provisioner_id, worker_type, use_proxy=False):
    url_tmpl = liburls.api(
        get_root_url(use_proxy), "purge-cache", "v1", "purge-cache/{}/{}"
    )
    return url_tmpl.format(provisioner_id, worker_type)


def purge_cache(provisioner_id, worker_type, cache_name, use_proxy=False):
    """Requests a cache purge from the purge-caches service."""
    if testing:
        logger.info(f"Would have purged {provisioner_id}/{worker_type}/{cache_name}.")
    else:
        logger.info(f"Purging {provisioner_id}/{worker_type}/{cache_name}.")
        purge_cache_url = get_purge_cache_url(provisioner_id, worker_type, use_proxy)
        _do_request(purge_cache_url, json={"cacheName": cache_name})


def send_email(address, subject, content, link, use_proxy=False):
    """Sends an email using the notify service"""
    logger.info(f"Sending email to {address}.")
    url = liburls.api(get_root_url(use_proxy), "notify", "v1", "email")
    _do_request(
        url,
        json={
            "address": address,
            "subject": subject,
            "content": content,
            "link": link,
        },
    )


def list_task_group_incomplete_tasks(task_group_id):
    """Generate the incomplete tasks in a task group"""
    params = {}
    while True:
        url = liburls.api(
            get_root_url(False),
            "queue",
            "v1",
            f"task-group/{task_group_id}/list",
        )
        resp = _do_request(url, method="get", params=params).json()
        for task in [t["status"] for t in resp["tasks"]]:
            if task["state"] in ["running", "pending", "unscheduled"]:
                yield task["taskId"]
        if resp.get("continuationToken"):
            params = {"continuationToken": resp.get("continuationToken")}
        else:
            break


@functools.lru_cache(maxsize=None)
def _get_deps(task_ids, use_proxy):
    upstream_tasks = {}
    for task_id in task_ids:
        task_def = get_task_definition(task_id, use_proxy)
        upstream_tasks[task_def["metadata"]["name"]] = task_id

        upstream_tasks.update(_get_deps(tuple(task_def["dependencies"]), use_proxy))

    return upstream_tasks


def get_ancestors(
    task_ids: Union[List[str], str], use_proxy: bool = False
) -> Dict[str, str]:
    """Gets the ancestor tasks of the given task_ids as a dictionary of label -> taskid.

    Args:
        task_ids (str or [str]): A single task id or a list of task ids to find the ancestors of.
        use_proxy (bool): See get_root_url.

    Returns:
        dict: A dict whose keys are task labels and values are task ids.
    """
    upstream_tasks: Dict[str, str] = {}

    if isinstance(task_ids, str):
        task_ids = [task_ids]

    for task_id in task_ids:
        task_def = get_task_definition(task_id, use_proxy)

        upstream_tasks.update(_get_deps(tuple(task_def["dependencies"]), use_proxy))

    return copy.deepcopy(upstream_tasks)
