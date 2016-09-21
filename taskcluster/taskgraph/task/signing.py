# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os

from . import base
from taskgraph.util.templates import Templates


logger = logging.getLogger(__name__)
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'


class SigningTask(base.Task):

    def __init__(self, kind, name, task, attributes):
        self.unsigned_artifact_label = task['unsigned-task']['label']
        super(SigningTask, self).__init__(kind, name, task=task['task'],
                                          attributes=attributes)

    @classmethod
    def load_tasks(cls, kind, path, config, params, loaded_tasks):
        root = os.path.abspath(path)

        tasks = []
        for filename in config.get('jobs-from', []):
            templates = Templates(root)
            jobs = templates.load(filename, {})

            for name, job in jobs.iteritems():
                for artifact in job['unsigned-task']['artifacts']:
                    url = ARTIFACT_URL.format('<{}>'.format('unsigned-artifact'), artifact)
                    job['task']['payload']['unsignedArtifacts'].append({
                        'task-reference': url
                    })
                attributes = job.setdefault('attributes', {})
                attributes.update({'kind': 'signing'})
                tasks.append(cls(kind, name, job, attributes=attributes))

        return tasks

    def get_dependencies(self, taskgraph):
        return [(self.unsigned_artifact_label, 'unsigned-artifact')]

    def optimize(self, params):
        return False, None
