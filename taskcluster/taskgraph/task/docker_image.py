# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging

from . import transform
from taskgraph.transforms.base import TransformSequence, TransformConfig
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

    @classmethod
    def from_json(cls, task_dict):
        docker_image_task = cls(kind='docker-image', task=task_dict)
        return docker_image_task
