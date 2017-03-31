# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the push-apk-breakpoint kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import functools

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema
from taskgraph.util.scriptworker import get_push_apk_breakpoint_worker_type
from taskgraph.util.push_apk import fill_labels_tranform, validate_jobs_schema_transform_partial, \
    validate_dependent_tasks_transform, delete_non_required_fields_transform, generate_dependencies
from voluptuous import Required


transforms = TransformSequence()

push_apk_breakpoint_description_schema = Schema({
    # the dependent task (object) for this beetmover job, used to inform beetmover.
    Required('dependent-tasks'): object,
    Required('name'): basestring,
    Required('label'): basestring,
    Required('description'): basestring,
    Required('attributes'): object,
    Required('worker-type'): None,
    Required('worker'): object,
    Required('treeherder'): object,
    Required('run-on-projects'): list,
    Required('deadline-after'): basestring,
})

validate_jobs_schema_transform = functools.partial(
    validate_jobs_schema_transform_partial,
    push_apk_breakpoint_description_schema,
    'PushApkBreakpoint'
)

transforms.add(fill_labels_tranform)
transforms.add(validate_jobs_schema_transform)
transforms.add(validate_dependent_tasks_transform)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        job['dependencies'] = generate_dependencies(job['dependent-tasks'])

        worker_type = get_push_apk_breakpoint_worker_type(config)
        job['worker-type'] = worker_type

        job['worker']['payload'] = {} if 'human' in worker_type else {
                'image': 'ubuntu:16.10',
                'command': [
                    '/bin/bash',
                    '-c',
                    'echo "Dummy task while while bug 1351664 is implemented"'
                ],
                'maxRunTime': 600,
            }

        yield job


transforms.add(delete_non_required_fields_transform)
