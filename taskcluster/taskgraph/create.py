# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import requests
import json
import collections
import logging

from slugid import nice as slugid

logger = logging.getLogger(__name__)

def create_tasks(taskgraph, label_to_taskid):
    # TODO: use the taskGroupId of the decision task
    task_group_id = slugid()
    taskid_to_label = {t: l for l, t in label_to_taskid.iteritems()}

    session = requests.Session()

    for task_id in taskgraph.graph.visit_postorder():
        task_def = taskgraph.tasks[task_id].task
        task_def['taskGroupId'] = task_group_id
        _create_task(session, task_id, taskid_to_label[task_id], task_def)

def _create_task(session, task_id, label, task_def):
    # create the task using 'http://taskcluster/queue', which is proxied to the queue service
    # with credentials appropriate to this job.
    logger.debug("Creating task with taskId {} for {}".format(task_id, label))
    res = session.put('http://taskcluster/queue/v1/task/{}'.format(task_id), data=json.dumps(task_def))
    if res.status_code != 200:
        try:
            logger.error(res.json()['message'])
        except:
            logger.error(res.text)
        res.raise_for_status()
