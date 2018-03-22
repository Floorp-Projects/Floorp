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
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema, validate_schema
from taskgraph.util.scriptworker import get_push_apk_scope
from taskgraph.util.taskcluster import get_artifact_prefix

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


REQUIRED_ARCHITECTURES = {
    'android-x86-nightly',
    'android-api-16-nightly',
}
PLATFORM_REGEX = re.compile(r'build-signing-android-(\S+)-nightly')


@transforms.add
def validate_jobs_schema_transform_partial(config, jobs):
    for job in jobs:
        label = job.get('label', '?no-label?')
        validate_schema(
            push_apk_description_schema, job,
            "In PushApk ({!r} kind) task for {!r}:".format(config.kind, label)
        )
        yield job


@transforms.add
def validate_dependent_tasks(_, jobs):
    for job in jobs:
        check_every_architecture_is_present_in_dependent_tasks(job['dependent-tasks'])
        yield job


def check_every_architecture_is_present_in_dependent_tasks(dependent_tasks):
    dep_platforms = set(t.attributes.get('build_platform') for t in dependent_tasks)
    missed_architectures = REQUIRED_ARCHITECTURES - dep_platforms
    if missed_architectures:
        raise Exception('''One or many required architectures are missing.

Required architectures: {}.
Given dependencies: {}.
'''.format(REQUIRED_ARCHITECTURES, dependent_tasks)
        )


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        job['dependencies'] = generate_dependencies(job['dependent-tasks'])
        job['worker']['upstream-artifacts'] = generate_upstream_artifacts(
            job, job['dependencies']
        )

        # Use the rc-google-play-track and rc-rollout-percentage in RC relpro flavors
        if config.params['release_type'] == 'rc':
            job['worker']['google-play-track'] = job['worker']['rc-google-play-track']
            job['worker']['rollout-percentage'] = job['worker']['rc-rollout-percentage']

        resolve_keyed_by(
            job, 'worker.google-play-track', item_name=job['name'],
            project=config.params['project']
        )
        resolve_keyed_by(
            job, 'worker.commit', item_name=job['name'],
            project=config.params['project']
        )

        resolve_keyed_by(
            job, 'worker.rollout-percentage', item_name=job['name'],
            project=config.params['project']
        )

        job['scopes'] = [get_push_apk_scope(config)]

        resolve_keyed_by(
            job, 'worker-type', item_name=job['name'],
            project=config.params['project']
        )

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
      if task_kind not in ('google-play-strings', 'beetmover-checksums')
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

        del(job['worker']['rc-google-play-track'])
        del(job['worker']['rc-rollout-percentage'])

        yield job
