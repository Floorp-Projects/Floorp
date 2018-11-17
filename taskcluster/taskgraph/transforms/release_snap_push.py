# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-snap-push kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema, validate_schema

from voluptuous import Optional, Required


transforms = TransformSequence()

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}


push_snap_description_schema = Schema({
    Required('name'): basestring,
    Required('job-from'): task_description_schema['job-from'],
    Required('dependencies'): task_description_schema['dependencies'],
    Required('description'): task_description_schema['description'],
    Required('treeherder'): task_description_schema['treeherder'],
    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('worker-type'): optionally_keyed_by('release-level', basestring),
    Required('worker'): object,
    Required('scopes'): optionally_keyed_by('project', [basestring]),
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
    Optional('extra'): task_description_schema['extra'],
})


@transforms.add
def validate_jobs_schema_transform(config, jobs):
    for job in jobs:
        label = job.get('label', '?no-label?')
        validate_schema(
            push_snap_description_schema, job,
            "In release_snap_push ({!r} kind) task for {!r}:".format(config.kind, label)
        )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        if len(job['dependencies']) != 1:
            raise Exception('Exactly 1 dependency is required')

        job['worker']['upstream-artifacts'] = generate_upstream_artifacts(job['dependencies'])

        resolve_keyed_by(job, 'scopes', item_name=job['name'], project=config.params['project'])
        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'],
            **{'release-level': config.params.release_level()}
        )

        yield job


def generate_upstream_artifacts(dependencies):
    return [{
        'taskId': {'task-reference': '<{}>'.format(task_kind)},
        # TODO bug 1417960
        'taskType': 'build',
        'paths': ['public/build/target.snap'],
    } for task_kind in dependencies.keys()]
