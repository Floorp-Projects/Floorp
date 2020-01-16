# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-sign-and-push task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import resolve_keyed_by, optionally_keyed_by
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required

transforms = TransformSequence()

langpack_sign_push_description_schema = schema.extend({
    Required('label'): basestring,
    Required('description'): basestring,
    Required('worker-type'): optionally_keyed_by('release-level', basestring),
    Required('worker'): {
        Required('implementation'): 'push-addons',
        Required('channel'): optionally_keyed_by(
            'project',
            optionally_keyed_by('platform', Any('listed', 'unlisted'))),
        Required('upstream-artifacts'): None,   # Processed here below
    },

    Required('run-on-projects'): [],
    Required('scopes'): optionally_keyed_by('release-level', [basestring]),
    Required('shipping-phase'): task_description_schema['shipping-phase'],
    Required('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def set_label(config, jobs):
    for job in jobs:
        label = 'push-langpacks-{}'.format(job['primary-dependency'].label)
        job['label'] = label

        yield job


transforms.add_validate(langpack_sign_push_description_schema)


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'worker-type', item_name=job['label'],
            **{'release-level': config.params.release_level()}
        )
        resolve_keyed_by(
            job, 'scopes', item_name=job['label'],
            **{'release-level': config.params.release_level()}
        )
        resolve_keyed_by(
            job, 'worker.channel', item_name=job['label'],
            project=config.params['project'],
            platform=job['primary-dependency'].attributes['build_platform'],
        )

        yield job


@transforms.add
def copy_attributes(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        job['attributes'] = copy_attributes_from_dependent_job(dep_job)
        job['attributes']['chunk_locales'] = dep_job.attributes.get('chunk_locales', ['en-US'])

        yield job


@transforms.add
def filter_out_macos_jobs_but_mac_only_locales(config, jobs):
    for job in jobs:
        build_platform = job['primary-dependency'].attributes.get('build_platform')

        if build_platform in (
                'linux64-nightly', 'linux64-devedition-nightly',
                'linux64-shippable'):
            yield job
        elif build_platform in (
                'macosx64-nightly', 'macosx64-devedition-nightly',
                'macosx64-shippable') and \
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
        dep_job = job['primary-dependency']

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        treeherder.setdefault('symbol', 'langpack(SnP{})'.format(
            job['attributes'].get('l10n_chunk', '')
        ))

        job['description'] = job['description'].format(
            locales='/'.join(job['attributes']['chunk_locales']),
        )

        job['dependencies'] = {dep_job.kind: dep_job.label}
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
        del job['primary-dependency']

        yield job
