# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import datetime
import functools
import yaml
import requests
import logging
from mozbuild.util import memoize
from requests.packages.urllib3.util.retry import Retry
from requests.adapters import HTTPAdapter
from taskgraph.task import Task

_PUBLIC_TC_ARTIFACT_LOCATION = \
        'https://queue.taskcluster.net/v1/task/{task_id}/artifacts/{artifact_prefix}/{postfix}'

_PRIVATE_TC_ARTIFACT_LOCATION = \
        'http://taskcluster/queue/v1/task/{task_id}/artifacts/{artifact_prefix}/{postfix}'

logger = logging.getLogger(__name__)

# this is set to true for `mach taskgraph action-callback --test`
testing = False


@memoize
def get_session():
    session = requests.Session()
    retry = Retry(total=5, backoff_factor=0.1,
                  status_forcelist=[500, 502, 503, 504])
    session.mount('http://', HTTPAdapter(max_retries=retry))
    session.mount('https://', HTTPAdapter(max_retries=retry))
    return session


def _do_request(url, **kwargs):
    session = get_session()
    if kwargs:
        response = session.post(url, **kwargs)
    else:
        response = session.get(url, stream=True)
    if response.status_code >= 400:
        # Consume content before raise_for_status, so that the connection can be
        # reused.
        response.content
    response.raise_for_status()
    return response


def _handle_artifact(path, response):
    if path.endswith('.json'):
        return response.json()
    if path.endswith('.yml'):
        return yaml.load(response.text)
    response.raw.read = functools.partial(response.raw.read,
                                          decode_content=True)
    return response.raw


def get_artifact_url(task_id, path, use_proxy=False):
    ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
    if use_proxy:
        # Until Bug 1405889 is deployed, we can't download directly
        # from the taskcluster-proxy.  Work around by using the /bewit
        # endpoint instead.
        data = ARTIFACT_URL.format(task_id, path)
        # The bewit URL is the body of a 303 redirect, which we don't
        # want to follow (which fetches a potentially large resource).
        response = _do_request('http://taskcluster/bewit', data=data, allow_redirects=False)
        return response.text
    return ARTIFACT_URL.format(task_id, path)


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
    response = _do_request(get_artifact_url(task_id, '', use_proxy).rstrip('/'))
    return response.json()['artifacts']


def get_artifact_prefix(task):
    prefix = None
    if isinstance(task, dict):
        prefix = task.get('attributes', {}).get("artifact_prefix")
    elif isinstance(task, Task):
        prefix = task.attributes.get("artifact_prefix")
    return prefix or "public/build"


def get_artifact_path(task, path):
    return "{}/{}".format(get_artifact_prefix(task), path)


def get_index_url(index_path, use_proxy=False, multiple=False):
    if use_proxy:
        INDEX_URL = 'http://taskcluster/index/v1/task{}/{}'
    else:
        INDEX_URL = 'https://index.taskcluster.net/v1/task{}/{}'
    return INDEX_URL.format('s' if multiple else '', index_path)


def find_task_id(index_path, use_proxy=False):
    try:
        response = _do_request(get_index_url(index_path, use_proxy))
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 404:
            raise KeyError("index path {} not found".format(index_path))
        raise
    return response.json()['taskId']


def get_artifact_from_index(index_path, artifact_path, use_proxy=False):
    full_path = index_path + '/artifacts/' + artifact_path
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
        response = _do_request(get_index_url(index_path, use_proxy, multiple=True), json=data)
        response = response.json()
        results += response['tasks']
        if response.get('continuationToken'):
            data = {'continuationToken': response.get('continuationToken')}
        else:
            break

    # We can sort on expires because in the general case
    # all of these tasks should be created with the same expires time so they end up in
    # order from earliest to latest action. If more correctness is needed, consider
    # fetching each task and sorting on the created date.
    results.sort(key=lambda t: datetime.datetime.strptime(t['expires'], '%Y-%m-%dT%H:%M:%S.%fZ'))
    return [t['taskId'] for t in results]


def get_task_url(task_id, use_proxy=False):
    if use_proxy:
        TASK_URL = 'http://taskcluster/queue/v1/task/{}'
    else:
        TASK_URL = 'https://queue.taskcluster.net/v1/task/{}'
    return TASK_URL.format(task_id)


def get_task_definition(task_id, use_proxy=False):
    response = _do_request(get_task_url(task_id, use_proxy))
    return response.json()


def cancel_task(task_id, use_proxy=False):
    """Cancels a task given a task_id. In testing mode, just logs that it would
    have cancelled."""
    if testing:
        logger.info('Would have cancelled {}.'.format(task_id))
    else:
        _do_request(get_task_url(task_id, use_proxy) + '/cancel', json={})


def status_task(task_id, use_proxy=False):
    """Gets the status of a task given a task_id. In testing mode, just logs that it would
    have retrieved status."""
    if testing:
        logger.info('Would have gotten status for {}.'.format(task_id))
    else:
        resp = _do_request(get_task_url(task_id, use_proxy) + '/status')
        status = resp.json().get("status", {}).get('state') or 'unknown'
        return status


def rerun_task(task_id):
    """Reruns a task given a task_id. In testing mode, just logs that it would
    have reran."""
    if testing:
        logger.info('Would have rerun {}.'.format(task_id))
    else:
        _do_request(get_task_url(task_id, use_proxy=True) + '/rerun', json={})


def get_purge_cache_url(provisioner_id, worker_type, use_proxy=False):
    if use_proxy:
        TASK_URL = 'http://taskcluster/purge-cache/v1/purge-cache/{}/{}'
    else:
        TASK_URL = 'https://purge-cache.taskcluster.net/v1/purge-cache/{}/{}'
    return TASK_URL.format(provisioner_id, worker_type)


def purge_cache(provisioner_id, worker_type, cache_name, use_proxy=False):
    """Requests a cache purge from the purge-caches service."""
    if testing:
        logger.info('Would have purged {}/{}/{}.'.format(provisioner_id, worker_type, cache_name))
    else:
        logger.info('Purging {}/{}/{}.'.format(provisioner_id, worker_type, cache_name))
        purge_cache_url = get_purge_cache_url(provisioner_id, worker_type, use_proxy)
        _do_request(purge_cache_url, json={'cacheName': cache_name})


def get_taskcluster_artifact_prefix(task, task_id, postfix='', locale=None, force_private=False):
    if locale:
        postfix = '{}/{}'.format(locale, postfix)

    artifact_prefix = get_artifact_prefix(task)
    if artifact_prefix == 'public/build' and not force_private:
        tmpl = _PUBLIC_TC_ARTIFACT_LOCATION
    else:
        tmpl = _PRIVATE_TC_ARTIFACT_LOCATION

    return tmpl.format(
        task_id=task_id, postfix=postfix, artifact_prefix=artifact_prefix
    )
