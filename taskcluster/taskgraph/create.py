# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import requests
import json
import collections

from slugid import nice as slugid

def create_tasks(taskgraph):
    # TODO: use the taskGroupId of the decision task
    task_group_id = slugid()
    label_to_taskid = collections.defaultdict(slugid)

    session = requests.Session()

    for label in taskgraph.graph.visit_postorder():
        task = taskgraph.tasks[label]
        deps_by_name = {
            n: label_to_taskid[r]
            for (l, r, n) in taskgraph.graph.edges
            if l == label}
        task_def = task.kind.get_task_definition(task, deps_by_name)
        task_def['taskGroupId'] = task_group_id
        task_def['dependencies'] = deps_by_name.values()
        task_def['requires'] = 'all-completed'

        _create_task(session, label_to_taskid[label], label, task_def)

def _create_task(session, task_id, label, task_def):
    # create the task using 'http://taskcluster/queue', which is proxied to the queue service
    # with credentials appropriate to this job.
    print("Creating task with taskId {} for {}".format(task_id, label))
    res = session.put('http://taskcluster/queue/v1/task/{}'.format(task_id), data=json.dumps(task_def))
    if res.status_code != 200:
        try:
            print(res.json()['message'])
        except:
            print(res.text)
        res.raise_for_status()
