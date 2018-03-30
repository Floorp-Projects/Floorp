# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-sign-and-push task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema, resolve_keyed_by, optionally_keyed_by
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required


transforms = TransformSequence()

task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}


transforms = TransformSequence()


langpack_sign_push_description_schema = Schema({
    Required('dependent-task'): object,
    Required('label'): basestring,
    Required('description'): basestring,
    Required('worker-type'): optionally_keyed_by('project', basestring),
    Required('worker'): {
        Required('implementation'): 'sign-and-push-addons',
        Required('channel'): optionally_keyed_by('project', Any('listed', 'unlisted')),
        Required('upstream-artifacts'): None,   # Processed here below
    },

    Required('run-on-projects'): [],
    Required('scopes'): optionally_keyed_by('project', [basestring]),
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def set_label(config, jobs):
    for job in jobs:
        label = 'sign-and-push-langpacks-{}'.format(job['dependent-task'].label)
        job['label'] = label

        yield job


@transforms.add
def validate(config, jobs):
    for job in jobs:
        validate_schema(
            langpack_sign_push_description_schema, job,
            'In sign-and-push-langpacks ({} kind) task for {}:'.format(config.kind, job['label'])
        )
        yield job


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'worker-type', item_name=job['label'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'scopes', item_name=job['label'], project=config.params['project']
        )
        resolve_keyed_by(
            job, 'worker.channel', item_name=job['label'], project=config.params['project']
        )

        yield job


@transforms.add
def copy_attributes(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        job['attributes'] = copy_attributes_from_dependent_job(dep_job)
        job['attributes']['chunk_locales'] = dep_job.attributes.get('chunk_locales', ['en-US'])

        yield job


@transforms.add
def filter_out_macos_jobs_but_mac_only_locales(config, jobs):
    for job in jobs:
        build_platform = job['dependent-task'].attributes.get('build_platform')

        if build_platform == 'linux64-nightly':
            yield job
        elif build_platform == 'macosx64-nightly' and \
                'ja-JP-mac' in job['attributes']['chunk_locales']:
            # Other locales of the same job shouldn't be processed
            job['attributes']['chunk_locales'] = ['ja-JP-mac']
            job['label'] = job['label'].replace(
                job['attributes']['l10n_chunk'], 'ja-JP-mac'
            )
            yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'langpack(SnP{})'.format(
            job['attributes'].get('l10n_chunk', '')
        ))
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform', '{}/opt'.format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        job['description'] = job['description'].format(
            locales='/'.join(job['attributes']['chunk_locales']),
        )

        job['dependencies'] = {
            str(dep_job.kind): dep_job.label
        }
        job['treeherder'] = treeherder

        yield job


def generate_upstream_artifacts(upstream_task_ref, locales):
    return [{
        'taskId': {'task-reference': upstream_task_ref},
        'taskType': 'build',
        'paths': [
            'public/build{locale}/target.langpack.xpi'.format(
                locale='' if locale == 'en-US' else '/' + locale
            )
            for locale in locales
        ],
    }]


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        upstream_task_ref = get_upstream_task_ref(job, expected_kinds=('build', 'nightly-l10n'))

        job['worker']['upstream-artifacts'] = generate_upstream_artifacts(
            upstream_task_ref, job['attributes']['chunk_locales']
        )

        yield job


def get_upstream_task_ref(job, expected_kinds):
    upstream_tasks = [
        job_kind
        for job_kind in job['dependencies'].keys()
        if job_kind in expected_kinds
    ]

    if len(upstream_tasks) > 1:
        raise Exception('Only one dependency expected')

    return '<{}>'.format(upstream_tasks[0])


@transforms.add
def strip_unused_data(config, jobs):
    for job in jobs:
        del job['dependent-task']

        yield job
