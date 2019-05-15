# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the push-apk-checks kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.transforms.push_apk import (
    generate_dependencies,
    delete_non_required_fields,
)
from taskgraph.transforms.job.mozharness_test import get_artifact_url
from taskgraph.util.schema import Schema

from voluptuous import Required

transforms = TransformSequence()
transforms.add_validate(Schema({
    Required('dependent-tasks'): object,
    Required('name'): basestring,
    Required('label'): task_description_schema['label'],
    Required('description'): task_description_schema['description'],
    Required('job-from'): task_description_schema['job-from'],
    Required('attributes'): object,
    Required('treeherder'): task_description_schema['treeherder'],
    Required('package-name'): basestring,
    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('worker-type'): basestring,
    Required('worker'): object,
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
}))


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        job['dependencies'] = generate_dependencies(job['dependent-tasks'])
        dependencies_labels = job['dependencies'].keys()

        commands = [
            'curl --location "{}" > "<{}>.apk"'.format(
                get_artifact_url('<{}>'.format(task_label), 'public/build/target.apk'),
                task_label,
            )
            for task_label in dependencies_labels
        ]

        check_apks_command = (
            'python3 ./mozapkpublisher/check_apks.py --expected-package-name "{}" {}'.format(
                job['package-name'],
                ' '.join(['"<{}>.apk"'.format(task_label) for task_label in dependencies_labels])
            )
        )
        commands.append(check_apks_command)

        job['worker']['command'] = [
            '/bin/bash',
            '-cx',
            {'task-reference': ' && '.join(commands)},
        ]

        yield job


@transforms.add
def delete_non_required_fields_(config, jobs):
    for job in jobs:
        del job['package-name']
        yield job


transforms.add(delete_non_required_fields)
