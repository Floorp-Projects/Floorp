# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import os
import urllib2

from . import transform
from taskgraph.util.docker import INDEX_PREFIX
from taskgraph.transforms.base import TransformSequence, TransformConfig
from taskgraph.util.taskcluster import get_artifact_url
from taskgraph.util.python_path import find_object

logger = logging.getLogger(__name__)


def transform_inputs(inputs, kind, path, config, params, loaded_tasks):
    """
    Transform a sequence of inputs according to the transform configuration.
    """
    transforms = TransformSequence()
    for xform_path in config['transforms']:
        transform = find_object(xform_path)
        transforms.add(transform)

    # perform the transformations
    trans_config = TransformConfig(kind, path, config, params)
    tasks = [DockerImageTask(kind, t)
             for t in transforms(trans_config, inputs)]
    return tasks


def load_tasks(kind, path, config, params, loaded_tasks):
    return transform_inputs(
        transform.get_inputs(kind, path, config, params, loaded_tasks),
        kind, path, config, params, loaded_tasks)


class DockerImageTask(transform.TransformTask):
    def get_dependencies(self, taskgraph):
        return []

    def optimize(self, params):
        optimized, taskId = super(DockerImageTask, self).optimize(params)
        if optimized and taskId:
            try:
                # Only return the task ID if the artifact exists for the indexed
                # task.
                request = urllib2.Request(get_artifact_url(
                    taskId, 'public/image.tar.zst',
                    use_proxy=bool(os.environ.get('TASK_ID'))))
                request.get_method = lambda: 'HEAD'
                urllib2.urlopen(request)

                # HEAD success on the artifact is enough
                return True, taskId
            except urllib2.HTTPError:
                pass

        return False, None

    @classmethod
    def from_json(cls, task_dict):
        # Generating index_paths for optimization
        imgMeta = task_dict['task']['extra']['imageMeta']
        image_name = imgMeta['imageName']
        context_hash = imgMeta['contextHash']
        index_paths = ['{}.level-{}.{}.hash.{}'.format(
            INDEX_PREFIX, level, image_name, context_hash)
            for level in reversed(range(int(imgMeta['level']), 4))]
        task_dict['index_paths'] = index_paths
        docker_image_task = cls(kind='docker-image', task=task_dict)
        return docker_image_task
