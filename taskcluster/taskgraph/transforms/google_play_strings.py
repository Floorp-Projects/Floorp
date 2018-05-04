# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the push-apk kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.schema import resolve_keyed_by, Schema, validate_schema
from taskgraph.util.scriptworker import get_release_config

from voluptuous import Required


transforms = TransformSequence()

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

google_play_description_schema = Schema({
    Required('name'): basestring,
    Required('label'): task_description_schema['label'],
    Required('description'): task_description_schema['description'],
    Required('job-from'): task_description_schema['job-from'],
    Required('attributes'): task_description_schema['attributes'],
    Required('treeherder'): task_description_schema['treeherder'],
    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
    Required('worker-type'): task_description_schema['worker-type'],
    Required('worker'): object,
})


@transforms.add
def validate_jobs_schema(config, jobs):
    for job in jobs:
        job['label'] = job['name']
        validate_schema(
            google_play_description_schema, job,
            "In GooglePlayStrings ({!r} kind) task for {!r}:".format(config.kind, job['label'])
        )
        yield job


@transforms.add
def set_worker_data(config, jobs):
    for job in jobs:
        worker = job['worker']

        env = worker.setdefault('env', {})
        resolve_keyed_by(
            env, 'PACKAGE_NAME', item_name=job['name'],
            project=config.params['project']
        )

        resolve_keyed_by(
            env, 'MAJOR_VERSION_NUMBER', item_name=job['name'],
            project=config.params['project']
        )
        if env.get('MAJOR_VERSION_NUMBER'):
            release_config = get_release_config(config)
            major_version = release_config['version'].split('.')[0]
            env['MAJOR_VERSION_NUMBER'] = env['MAJOR_VERSION_NUMBER'].format(
                major_version=major_version
            )
        else:
            del env['MAJOR_VERSION_NUMBER']

        yield job
