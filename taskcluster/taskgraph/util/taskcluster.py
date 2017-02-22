# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import functools
import os
import yaml
import requests
from mozbuild.util import memoize
from requests.packages.urllib3.util.retry import Retry
from requests.adapters import HTTPAdapter


# if running in a task, prefer to use the taskcluster proxy
# (http://taskcluster/), otherwise hit the services directly
if os.environ.get('TASK_ID'):
    INDEX_URL = 'http://taskcluster/index/v1/task/{}'
    ARTIFACT_URL = 'http://taskcluster/queue/v1/task/{}/artifacts/{}'
else:
    INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'
    ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'


@memoize
def _get_session():
    session = requests.Session()
    retry = Retry(total=5, backoff_factor=0.1,
                  status_forcelist=[500, 502, 503, 504])
    session.mount('http://', HTTPAdapter(max_retries=retry))
    session.mount('https://', HTTPAdapter(max_retries=retry))
    return session


def _do_request(url):
    session = _get_session()
    return session.get(url, stream=True)


def get_artifact_url(task_id, path):
    return ARTIFACT_URL.format(task_id, path)


def get_artifact(task_id, path):
    """
    Returns the artifact with the given path for the given task id.

    If the path ends with ".json" or ".yml", the content is deserialized as,
    respectively, json or yaml, and the corresponding python data (usually
    dict) is returned.
    For other types of content, a file-like object is returned.
    """
    response = _do_request(get_artifact_url(task_id, path))
    response.raise_for_status()
    if path.endswith('.json'):
        return response.json()
    if path.endswith('.yml'):
        return yaml.load(response.text)
    response.raw.read = functools.partial(response.raw.read,
                                          decode_content=True)
    return response.raw


def list_artifacts(task_id):
    response = _do_request(get_artifact_url(task_id, '').rstrip('/'))
    response.raise_for_status()
    return response.json()['artifacts']


def find_task_id(index_path):
    response = _do_request(INDEX_URL.format(index_path))
    response.raise_for_status()
    return response.json()['taskId']
