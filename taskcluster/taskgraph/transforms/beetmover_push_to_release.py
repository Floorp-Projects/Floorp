# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover-push-to-release task into a task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import (
     validate_schema, Schema,
)
from taskgraph.util.scriptworker import (
    get_beetmover_bucket_scope, get_beetmover_action_scope,
    get_worker_type_for_scope,
)
from taskgraph.transforms.job import job_description_schema
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}
job_description_schema = {str(k): v for k, v in job_description_schema.schema.iteritems()}

transforms = TransformSequence()

taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

beetmover_push_to_release_description_schema = Schema({
    Required('name'): basestring,
    Required('product'): basestring,
    Required('treeherder-platform'): basestring,
    Optional('attributes'): {basestring: object},
    Optional('job-from'): task_description_schema['job-from'],
    Optional('run'): {basestring: object},
    Optional('run-on-projects'): task_description_schema['run-on-projects'],
    Optional('dependencies'): {basestring: taskref_or_string},
    Optional('index'): {basestring: basestring},
    Optional('routes'): [basestring],
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
    Optional('extra'): task_description_schema['extra'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job['name']
        validate_schema(
            beetmover_push_to_release_description_schema, job,
            "In beetmover-push-to-release ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_beetmover_push_to_release_description(config, jobs):
    for job in jobs:
        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'Rel(BM-C)')
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')
        treeherder.setdefault('platform', job['treeherder-platform'])

        label = job['name']
        description = (
            "Beetmover push to release for '{product}'".format(
                product=job['product']
            )
        )

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, bucket_scope),
            'scopes': [bucket_scope, action_scope],
            'product': job['product'],
            'dependencies': job['dependencies'],
            'attributes': job.get('attributes', {}),
            'run-on-projects': job.get('run-on-projects'),
            'treeherder': treeherder,
            'shipping-phase': job.get('shipping-phase', 'push'),
            'shipping-product': job.get('shipping-product'),
            'routes': job.get('routes', []),
            'extra': job.get('extra', {}),
        }

        yield task


@transforms.add
def make_beetmover_push_to_release_worker(config, jobs):
    for job in jobs:
        worker = {
            'implementation': 'beetmover-push-to-release',
            'product': job['product'],
        }
        job["worker"] = worker
        del(job['product'])

        yield job
