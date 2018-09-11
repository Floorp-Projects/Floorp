# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import \
    craft_release_properties as beetmover_craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema, resolve_keyed_by, optionally_keyed_by
from taskgraph.util.scriptworker import (get_phase,
                                         get_worker_type_for_scope)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional


_ARTIFACT_ID_PER_PLATFORM = {
    'android-aarch64': 'geckoview{update_channel}-arm64-v8a',
    'android-api-16': 'geckoview{update_channel}-armeabi-v7a',
    'android-x86': 'geckoview{update_channel}-x86',
}

_MOZ_UPDATE_CHANNEL_PER_BRANCH = {
    'mozilla-release': '',
    'mozilla-beta': '-beta',
    'mozilla-central': '-nightly',
    'try': '-nightly-try',
    'maple': '-nightly-maple',
}

task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

beetmover_description_schema = Schema({
    Required('dependent-task'): object,
    Required('depname', default='build'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],

    Optional('bucket-scope'): optionally_keyed_by('release-level', basestring),
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            beetmover_description_schema, job,
            "In beetmover-geckoview ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'BM-gv')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              '{}/opt'.format(dep_th_platform))
        treeherder.setdefault('tier', 2)
        treeherder.setdefault('kind', 'build')
        label = job['label']
        description = (
            "Beetmover submission for geckoview"
            "{build_platform}/{build_type}'".format(
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)

        if job.get('locale'):
            attributes['locale'] = job['locale']

        resolve_keyed_by(
            job, 'bucket-scope', item_name=job['label'],
            **{'release-level': config.params.release_level()}
        )

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, job['bucket-scope']),
            'scopes': [job['bucket-scope'], 'project:releng:beetmover:action:push-to-maven'],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': ['mozilla-central'],
            'treeherder': treeherder,
            'shipping-phase': job.get('shipping-phase', get_phase(config)),
        }

        yield task


def generate_upstream_artifacts(build_task_ref):
    return [{
        'taskId': {'task-reference': build_task_ref},
        'taskType': 'build',
        'paths': ['public/build/target.maven.zip'],
        'zipExtract': True,
    }]


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = len(job['dependencies']) == 1 and 'build' in job['dependencies']
        if not valid_beetmover_job:
            raise NotImplementedError(
                'Beetmover-geckoview must have a single dependency. Got: {}'.format(
                    job['dependencies']
                )
            )

        build_task = list(job["dependencies"].keys())[0]
        build_task_ref = "<" + str(build_task) + ">"

        worker = {
            'implementation': 'beetmover-maven',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(build_task_ref)
        }

        job["worker"] = worker

        yield job


def craft_release_properties(config, job):
    props = beetmover_craft_release_properties(config, job)

    platform = props['platform']
    update_channel = _MOZ_UPDATE_CHANNEL_PER_BRANCH.get(
        props['branch'], '-UNKNOWN_MOZ_UPDATE_CHANNEL'
    )
    artifact_id = _ARTIFACT_ID_PER_PLATFORM[platform].format(update_channel=update_channel)
    props['artifact-id'] = artifact_id
    props['app-name'] = 'geckoview'     # this beetmover job is not about pushing Fennec

    return props
