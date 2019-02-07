# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Tests for the 'job' transform subsystem.
"""

from __future__ import absolute_import, print_function, unicode_literals

import os
from copy import deepcopy

import pytest
from mozunit import main

from taskgraph import GECKO
from taskgraph.config import load_graph_config
from taskgraph.transforms import job
from taskgraph.transforms.base import TransformConfig
from taskgraph.transforms.job.common import add_cache
from taskgraph.transforms.task import payload_builders
from taskgraph.util.schema import Schema, validate_schema
from taskgraph.util.workertypes import worker_type_implementation

here = os.path.abspath(os.path.dirname(__file__))


TASK_DEFAULTS = {
    'description': 'fake description',
    'label': 'fake-task-label',
    'run': {
        'using': 'run-task',
    },
}


@pytest.fixture(scope='module')
def config():
    graph_config = load_graph_config(os.path.join(GECKO, 'taskcluster', 'ci'))
    return TransformConfig('job_test', here, {}, {}, [], graph_config)


@pytest.fixture()
def transform(monkeypatch, config):
    """Run the job transforms on the specified task but return the inputs to
    `configure_taskdesc_for_run` without executing it.

    This gives test functions an easy way to generate the inputs required for
    many of the `run_using` subsystems.
    """

    def inner(task_input):
        task = deepcopy(TASK_DEFAULTS)
        task.update(task_input)
        frozen_args = []

        def _configure_taskdesc_for_run(*args):
            frozen_args.extend(args)

        monkeypatch.setattr(job, 'configure_taskdesc_for_run', _configure_taskdesc_for_run)

        for func in job.transforms._transforms:
            task = list(func(config, [task]))[0]

        return frozen_args

    return inner


@pytest.mark.parametrize('task', [
    {'worker-type': 'aws-provisioner-v1/gecko-1-b-linux'},
    {'worker-type': 'releng-hardware/gecko-t-win10-64-hw'},
], ids=lambda t: worker_type_implementation(t['worker-type'])[0])
def test_worker_caches(task, transform):
    config, job, taskdesc, impl = transform(task)
    add_cache(job, taskdesc, 'cache1', '/cache1')
    add_cache(job, taskdesc, 'cache2', '/cache2', skip_untrusted=True)

    if impl != 'docker-worker':
        pytest.xfail("caches not implemented for '{}'".format(impl))

    key = 'caches'
    assert key in taskdesc['worker']
    assert len(taskdesc['worker'][key]) == 2

    # Create a new schema object with just the part relevant to caches.
    partial_schema = Schema(payload_builders[impl].schema.schema.schema[key])
    validate_schema(partial_schema, taskdesc['worker'][key], "validation error")


if __name__ == '__main__':
    main()
