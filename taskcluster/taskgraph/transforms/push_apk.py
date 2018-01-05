# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the push-apk kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import functools

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema
from taskgraph.util.scriptworker import get_push_apk_scope, get_push_apk_track, \
    get_push_apk_commit_option, get_push_apk_rollout_percentage
from taskgraph.util.push_apk import fill_labels_tranform, validate_jobs_schema_transform_partial, \
    validate_dependent_tasks_transform, delete_non_required_fields_transform, generate_dependencies

from voluptuous import Optional, Required


transforms = TransformSequence()

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}


push_apk_description_schema = Schema({
    Required('dependent-tasks'): object,
    Required('name'): basestring,
    Required('label'): task_description_schema['label'],
    Required('description'): task_description_schema['description'],
    Required('job-from'): task_description_schema['job-from'],
    Required('attributes'): task_description_schema['attributes'],
    Required('treeherder'): task_description_schema['treeherder'],
    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('worker-type'): optionally_keyed_by('project', basestring),
    Required('worker'): object,
    Required('scopes'): None,
    Required('requires'): task_description_schema['requires'],
    Required('deadline-after'): basestring,
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
    Optional('extra'): task_description_schema['extra'],
})

validate_jobs_schema_transform = functools.partial(
    validate_jobs_schema_transform_partial,
    push_apk_description_schema,
    'PushApk'
)

transforms.add(fill_labels_tranform)
transforms.add(validate_jobs_schema_transform)
transforms.add(validate_dependent_tasks_transform)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        job['dependencies'] = generate_dependencies(job['dependent-tasks'])
        job['worker']['upstream-artifacts'] = generate_upstream_artifacts(job['dependencies'])
        job['worker']['google-play-track'] = get_push_apk_track(config)
        job['worker']['commit'] = get_push_apk_commit_option(config)

        rollout_percentage = get_push_apk_rollout_percentage(config)
        if rollout_percentage is not None:
            job['worker']['rollout-percentage'] = rollout_percentage

        job['scopes'] = [get_push_apk_scope(config)]

        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'],
            project=config.params['project']
        )

        yield job


transforms.add(delete_non_required_fields_transform)


def generate_upstream_artifacts(dependencies):
    apks = [{
        'taskId': {'task-reference': '<{}>'.format(task_kind)},
        'taskType': 'signing',
        'paths': ['public/build/target.apk'],
    } for task_kind in dependencies.keys()
      if task_kind not in ('push-apk-breakpoint', 'google-play-strings')
    ]

    google_play_strings = [{
        'taskId': {'task-reference': '<{}>'.format(task_kind)},
        'taskType': 'build',
        'paths': ['public/google_play_strings.json'],
        'optional': True,
    } for task_kind in dependencies.keys()
      if 'google-play-strings' in task_kind
    ]

    return apks + google_play_strings
