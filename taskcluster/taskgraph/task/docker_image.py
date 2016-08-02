# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import json
import os
import re
import urllib2
import time

from . import base
from taskgraph.util.docker import (
    create_context_tar,
    docker_image,
    generate_context_hash,
)
from taskgraph.util.templates import Templates

logger = logging.getLogger(__name__)
GECKO = os.path.realpath(os.path.join(__file__, '..', '..', '..', '..'))
ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'
INDEX_URL = 'https://index.taskcluster.net/v1/task/{}'
INDEX_REGEX = r'index\.(docker\.images\.v1\.(.+)\.(.+)\.hash\.(.+))'


class DockerImageTask(base.Task):

    def __init__(self, *args, **kwargs):
        self.index_paths = kwargs.pop('index_paths')
        super(DockerImageTask, self).__init__(*args, **kwargs)

    def __eq__(self, other):
        return super(DockerImageTask, self).__eq__(other) and \
               self.index_paths == other.index_paths

    @classmethod
    def load_tasks(cls, kind, path, config, params, loaded_tasks):
        # TODO: make this match the pushdate (get it from a parameter rather than vcs)
        pushdate = time.strftime('%Y%m%d%H%M%S', time.gmtime())

        parameters = {
            'pushlog_id': params.get('pushlog_id', 0),
            'pushdate': pushdate,
            'pushtime': pushdate[8:],
            'year': pushdate[0:4],
            'month': pushdate[4:6],
            'day': pushdate[6:8],
            'project': params['project'],
            'docker_image': docker_image,
            'base_repository': params['base_repository'] or params['head_repository'],
            'head_repository': params['head_repository'],
            'head_ref': params['head_ref'] or params['head_rev'],
            'head_rev': params['head_rev'],
            'owner': params['owner'],
            'level': params['level'],
            'source': '{repo}file/{rev}/taskcluster/ci/docker-image/image.yml'
                      .format(repo=params['head_repository'], rev=params['head_rev']),
        }

        tasks = []
        templates = Templates(path)
        for image_name in config['images']:
            context_path = os.path.join('testing', 'docker', image_name)

            image_parameters = dict(parameters)
            image_parameters['context_path'] = context_path
            image_parameters['artifact_path'] = 'public/image.tar'
            image_parameters['image_name'] = image_name

            image_artifact_path = \
                "public/docker_image_contexts/{}/context.tar.gz".format(image_name)
            if os.environ.get('TASK_ID'):
                # We put image context tar balls in a different artifacts folder on the
                # Gecko decision task in order to have longer expiration dates for smaller artifacts
                destination = os.path.join(
                    os.environ['HOME'],
                    "docker_image_contexts/{}/context.tar.gz".format(image_name))
                image_parameters['context_url'] = ARTIFACT_URL.format(
                    os.environ['TASK_ID'], image_artifact_path)

                destination = os.path.abspath(destination)
                if not os.path.exists(os.path.dirname(destination)):
                    os.makedirs(os.path.dirname(destination))

                context_hash = create_context_tar(GECKO, context_path,
                                                  destination, image_name)
            else:
                # skip context generation since this isn't a decision task
                # TODO: generate context tarballs using subdirectory clones in
                # the image-building task so we don't have to worry about this.
                image_parameters['context_url'] = 'file:///tmp/' + image_artifact_path
                context_hash = generate_context_hash(GECKO, context_path, image_name)

            image_parameters['context_hash'] = context_hash

            image_task = templates.load('image.yml', image_parameters)

            attributes = {'image_name': image_name}

            # As an optimization, if the context hash exists for mozilla-central, that image
            # task ID will be used.  The reasoning behind this is that eventually everything ends
            # up on mozilla-central at some point if most tasks use this as a common image
            # for a given context hash, a worker within Taskcluster does not need to contain
            # the same image per branch.
            index_paths = ['docker.images.v1.{}.{}.hash.{}'.format(
                                project, image_name, context_hash)
                           for project in ['mozilla-central', params['project']]]

            tasks.append(cls(kind, 'build-docker-image-' + image_name,
                             task=image_task['task'], attributes=attributes,
                             index_paths=index_paths))

        return tasks

    def get_dependencies(self, taskgraph):
        return []

    def optimize(self):
        for index_path in self.index_paths:
            try:
                url = INDEX_URL.format(index_path)
                existing_task = json.load(urllib2.urlopen(url))
                # Only return the task ID if the artifact exists for the indexed
                # task.  Otherwise, continue on looking at each of the branches.  Method
                # continues trying other branches in case mozilla-central has an expired
                # artifact, but 'project' might not. Only return no task ID if all
                # branches have been tried
                request = urllib2.Request(
                    ARTIFACT_URL.format(existing_task['taskId'], 'public/image.tar'))
                request.get_method = lambda: 'HEAD'
                urllib2.urlopen(request)

                # HEAD success on the artifact is enough
                return True, existing_task['taskId']
            except urllib2.HTTPError:
                pass

        return False, None

    @classmethod
    def from_json(cls, task_dict):
        # Generating index_paths for optimization
        routes = task_dict['task']['routes']
        index_paths = []
        for route in routes:
            index_path_regex = re.compile(INDEX_REGEX)
            result = index_path_regex.search(route)
            if result is None:
                continue
            index_paths.append(result.group(1))
            index_paths.append(result.group(1).replace(result.group(2), 'mozilla-central'))
        docker_image_task = cls(kind='docker-image',
                                label=task_dict['label'],
                                attributes=task_dict['attributes'],
                                task=task_dict['task'],
                                index_paths=index_paths)
        return docker_image_task
