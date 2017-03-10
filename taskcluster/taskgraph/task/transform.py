# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import itertools

from . import base
from ..util.python_path import find_object
from ..util.templates import merge
from ..util.yaml import load_yaml

from ..transforms.base import TransformSequence, TransformConfig

logger = logging.getLogger(__name__)


def get_inputs(kind, path, config, params, loaded_tasks):
    """
    Get the input elements that will be transformed into tasks.  The
    elements themselves are free-form, and become the input to the first
    transform.

    By default, this reads jobs from the `jobs` key, or from yaml files
    named by `jobs-from`.  The entities are read from mappings, and the
    keys to those mappings are added in the `name` key of each entity.

    If there is a `job-defaults` config, then every job is merged with it.
    This provides a simple way to set default values for all jobs of a
    kind.  More complex defaults should be implemented with custom
    transforms.

    Other kind implementations can use a different get_inputs function to
    produce inputs and hand them to `transform_inputs`.
    """
    def jobs():
        defaults = config.get('job-defaults')
        jobs = config.get('jobs', {}).iteritems()
        jobs_from = itertools.chain.from_iterable(
            load_yaml(path, filename).iteritems()
            for filename in config.get('jobs-from', {}))
        for name, job in itertools.chain(jobs, jobs_from):
            if defaults:
                job = merge(defaults, job)
            yield name, job

    for name, job in jobs():
        job['name'] = name
        logger.debug("Generating tasks for {} {}".format(kind, name))
        yield job


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
    tasks = [base.Task(kind,
                       label=task_dict['label'],
                       attributes=task_dict['attributes'],
                       task=task_dict['task'],
                       optimizations=task_dict.get('optimizations'),
                       dependencies=task_dict.get('dependencies'))
             for task_dict in transforms(trans_config, inputs)]
    return tasks


def load_tasks(kind, path, config, params, loaded_tasks):
    return transform_inputs(
        get_inputs(kind, path, config, params, loaded_tasks),
        kind, path, config, params, loaded_tasks)
