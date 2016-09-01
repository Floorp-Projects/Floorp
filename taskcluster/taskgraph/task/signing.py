# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import json
import os
import time

from . import base
from taskgraph.util.templates import Templates


logger = logging.getLogger(__name__)
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'

class SigningTask(base.Task):

    def __init__(self, *args, **kwargs):
        super(SigningTask, self).__init__(*args, **kwargs)

    @classmethod
    def load_tasks(cls, kind, path, config, params, loaded_tasks):
        root = os.path.abspath(os.path.join(path, config['signing_path']))

        # get each nightly-fennec and add its name to this task
        fennec_tasks = [t for t in loaded_tasks if t.attributes.get('kind')
                        == 'nightly-fennec']

        tasks = []
        for fennec_task in fennec_tasks:
            templates = Templates(root)
            task = templates.load('signing.yml', {})

            artifacts = ['public/build/target.apk',
                         'public/build/en-US/target.apk']
            for artifact in artifacts:
                url = ARTIFACT_URL.format('<build-nightly-fennec>', artifact)
                task['task']['payload']['unsignedArtifacts'].append({
                    'task-reference': url
                })

            attributes = {'kind': 'signing'}
            tasks.append(cls(kind, 'signing-nightly-fennec', task=task['task'],
                             attributes=attributes))

        return tasks

    def get_dependencies(self, taskgraph):
        return [('build-nightly-fennec', 'build-nightly-fennec')]

    def optimize(self):
        return False, None
