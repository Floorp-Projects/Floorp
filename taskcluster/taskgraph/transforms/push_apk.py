# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the push-apk kind into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import re

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema
from taskgraph.util.scriptworker import get_push_apk_scope
from taskgraph.util.taskcluster import get_artifact_prefix

from voluptuous import Optional, Required


push_apk_description_schema = Schema({
    Required('dependent-tasks'): object,
    Required('name'): basestring,
    Required('label'): task_description_schema['label'],
    Required('description'): task_description_schema['description'],
    Required('job-from'): task_description_schema['job-from'],
    Required('attributes'): object,
    Required('treeherder'): task_description_schema['treeherder'],
    Required('run-on-projects'): task_description_schema['run-on-projects'],
    Required('worker-type'): optionally_keyed_by('release-level', basestring),
    Required('worker'): object,
    Required('scopes'): None,
    Required('requires'): task_description_schema['requires'],
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
    Optional('extra'): task_description_schema['extra'],
})


PLATFORM_REGEX = re.compile(r'build-signing-android-(\S+)/(\S+)')

transforms = TransformSequence()
transforms.add_validate(push_apk_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        job['dependencies'] = generate_dependencies(job['dependent-tasks'])
        job['worker']['upstream-artifacts'] = generate_upstream_artifacts(job, job['dependencies'])

        params_kwargs = {
            'release-level': config.params.release_level(),
            'release-type': config.params['release_type'],
        }

        for key in (
            'worker.commit',
            'worker.google-play-track',
            'worker.rollout-percentage',
            'worker-type',
        ):
            resolve_keyed_by(job, key, item_name=job['name'], **params_kwargs)

        job['scopes'] = [get_push_apk_scope(config, job['attributes'].get('release-type'))]

        yield job


def generate_dependencies(dependent_tasks):
    # Because we depend on several tasks that have the same kind, we introduce the platform
    dependencies = {}
    for task in dependent_tasks:
        platform_match = PLATFORM_REGEX.match(task.label)
        # platform_match is None when the google-play-string task is given, for instance
        task_kind = task.kind if platform_match is None else \
            '{}-{}'.format(task.kind, platform_match.group(1))
        dependencies[task_kind] = task.label
    return dependencies


def generate_upstream_artifacts(job, dependencies):
    artifact_prefix = get_artifact_prefix(job)
    apks = [{
        'taskId': {'task-reference': '<{}>'.format(task_kind)},
        'taskType': 'signing',
        'paths': ['{}/target.apk'.format(artifact_prefix)],
    } for task_kind in dependencies.keys()
      if task_kind not in ('google-play-strings', 'push-apk-checks')
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


@transforms.add
def delete_non_required_fields(_, jobs):
    for job in jobs:
        del job['name']
        del job['dependent-tasks']
        yield job
