# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import functools
import yaml
import requests
from mozbuild.util import memoize
from requests.packages.urllib3.util.retry import Retry
from requests.adapters import HTTPAdapter


@memoize
def get_session():
    session = requests.Session()
    retry = Retry(total=5, backoff_factor=0.1,
                  status_forcelist=[500, 502, 503, 504])
    session.mount('http://', HTTPAdapter(max_retries=retry))
    session.mount('https://', HTTPAdapter(max_retries=retry))
    return session


def _do_request(url):
    session = get_session()
    response = session.get(url, stream=True)
    if response.status_code >= 400:
        # Consume content before raise_for_status, so that the connection can be
        # reused.
        response.content
    response.raise_for_status()
    return response


def get_artifact_url(task_id, path, use_proxy=False):
    if use_proxy:
        ARTIFACT_URL = 'http://taskcluster/queue/v1/task/{}/artifacts/{}'
    else:
        ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
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
    if path.endswith('.json'):
        return response.json()
    if path.endswith('.yml'):
        return yaml.load(response.text)
    response.raw.read = functools.partial(response.raw.read,
                                          decode_content=True)
    return response.raw


def list_artifacts(task_id, use_proxy=False):
    response = _do_request(get_artifact_url(task_id, '', use_proxy).rstrip('/'))
    return response.json()['artifacts']


def get_index_url(index_path, use_proxy=False):
    if use_proxy:
        INDEX_URL = 'http://taskcluster/index/v1/task/{}'
    else:
        INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'
    return INDEX_URL.format(index_path)


def find_task_id(index_path, use_proxy=False):
    response = _do_request(get_index_url(index_path, use_proxy))
    return response.json()['taskId']
