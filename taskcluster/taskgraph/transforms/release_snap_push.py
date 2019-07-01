# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-snap-push kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema
from taskgraph.util.scriptworker import add_scope_prefix

from voluptuous import Optional, Required

push_snap_description_schema = Schema({
    Required('name'): basestring,
    Required('job-from'): task_description_schema['job-from'],
    Required('dependencies'): task_description_schema['dependencies'],
    Required('description'): task_description_schema['description'],
    Required('treeherder'): task_description_schema['treeherder'],
    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('worker-type'): optionally_keyed_by('release-level', basestring),
    Required('worker'): object,
    Optional('scopes'): [basestring],
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
    Optional('extra'): task_description_schema['extra'],
    Optional('attributes'): task_description_schema['attributes'],
})

transforms = TransformSequence()
transforms.add_validate(push_snap_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        if len(job['dependencies']) != 1:
            raise Exception('Exactly 1 dependency is required')

        job['worker']['upstream-artifacts'] = generate_upstream_artifacts(job['dependencies'])

        resolve_keyed_by(
            job, 'worker.channel', item_name=job['name'],
            **{'release-type': config.params['release_type']}
        )
        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'],
            **{'release-level': config.params.release_level()}
        )
        if config.params.release_level() == 'production':
            job.setdefault('scopes', []).append(
                add_scope_prefix(
                    config,
                    "snapcraft:firefox:{}".format(job['worker']['channel'].split('/')[0]),
                )
            )

        yield job


def generate_upstream_artifacts(dependencies):
    return [{
        'taskId': {'task-reference': '<{}>'.format(task_kind)},
        # TODO bug 1417960
        'taskType': 'build',
        'paths': ['public/build/target.snap'],
    } for task_kind in dependencies.keys()]
