# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import json
import os
import time

from . import base
from .legacy import query_vcs_info, decorate_task_json_routes, gaia_info, \
    mklabel
from taskgraph.util.templates import Templates
from taskgraph.util.docker import docker_image


logger = logging.getLogger(__name__)
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'


class NightlyFennecTask(base.Task):

    def __init__(self, *args, **kwargs):
        self.task_dict = kwargs.pop('task_dict')
        super(NightlyFennecTask, self).__init__(*args, **kwargs)

    @classmethod
    def load_tasks(cls, kind, path, config, params, loaded_tasks):
        root = os.path.abspath(os.path.join(path, config[
            'nightly_fennec_path']))

        project = params['project']

        # Set up the parameters, including the time of the build
        push_epoch = int(time.time())
        vcs_info = query_vcs_info(params['head_repository'], params['head_rev'])
        changed_files = set()
        if vcs_info:
            push_epoch = vcs_info.pushdate

            logger.debug(
                '{} commits influencing task scheduling:'.format(len(vcs_info.changesets)))
            for c in vcs_info.changesets:
                logger.debug("{cset} {desc}".format(
                    cset=c['node'][0:12],
                    desc=c['desc'].splitlines()[0].encode('ascii', 'ignore')))
                changed_files |= set(c['files'])

        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime(push_epoch))

        # Template parameters used when expanding the graph
        parameters = dict(gaia_info().items() + {
            'index': 'index',
            'project': project,
            'pushlog_id': params.get('pushlog_id', 0),
            'base_repository': params['base_repository'] or params['head_repository'],
            'docker_image': docker_image,
            'head_repository': params['head_repository'],
            'head_ref': params['head_ref'] or params['head_rev'],
            'head_rev': params['head_rev'],
            'pushdate': pushdate,
            'pushtime': pushdate[8:],
            'year': pushdate[0:4],
            'month': pushdate[4:6],
            'day': pushdate[6:8],
            'rank': push_epoch,
            'owner': params['owner'],
            'level': params['level'],
            'build_slugid': mklabel(),
            'source': '{repo}file/{rev}/taskcluster/ci/nightly-fennec'.format(
                repo=params['head_repository'], rev=params['head_rev']),
            'build_name': 'android',
            'build_type': 'opt',
            'build_product': 'mobile'
        }.items())

        routes_file = os.path.join(root, 'routes.json')
        with open(routes_file) as f:
            contents = json.load(f)
            json_routes = contents['routes']

        tasks = []
        templates = Templates(root)

        task = templates.load('android-api-15-nightly-build.yml', parameters)
        decorate_task_json_routes(task['task'], json_routes, parameters)

        attributes = {'kind': 'nightly-fennec'}
        tasks.append(cls(kind, 'build-nightly-fennec',
                     task=task['task'], attributes=attributes,
                         task_dict=task))

        # Convert to a dictionary of tasks.  The process above has invented a
        # taskId for each task, and we use those as the *labels* for the tasks;
        # taskgraph will later assign them new taskIds.
        return tasks

    def get_dependencies(self, taskgraph):
        deps = [(label, label) for label in self.task_dict.get('requires', [])]

        # add a dependency on an image task, if needed
        if 'docker-image' in self.task_dict:
            deps.append(('build-docker-image-{docker-image}'.format(**self.task_dict),
                         'docker-image'))

        return deps

    def optimize(self):
        return False, None
