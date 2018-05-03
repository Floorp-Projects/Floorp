# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema, optionally_keyed_by, resolve_keyed_by
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope)
from taskgraph.transforms.task import task_description_schema
from taskgraph.transforms.release_sign_and_push_langpacks import get_upstream_task_ref
from voluptuous import Required, Optional

import logging
import copy

logger = logging.getLogger(__name__)


task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}


transforms = TransformSequence()


beetmover_description_schema = Schema({
    # the dependent task (object) for this beetmover job, used to inform beetmover.
    Required('dependent-task'): object,

    # depname is used in taskref's to identify the taskID of the unsigned things
    Required('depname', default='build'): basestring,

    # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
    Optional('label'): basestring,

    # treeherder is allowed here to override any defaults we use for beetmover.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],

    Required('description'): basestring,
    Required('worker-type'): optionally_keyed_by('project', basestring),
    Required('run-on-projects'): [],

    # locale is passed only for l10n beetmoving
    Optional('locale'): basestring,
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def set_label(config, jobs):
    for job in jobs:
        job['label'] = job['dependent-task'].label.replace(
            'sign-and-push-langpacks', 'beetmover-signed-langpacks'
        )

        yield job


@transforms.add
def validate(config, jobs):
    for job in jobs:
        validate_schema(
            beetmover_description_schema, job,
            "In beetmover ({!r} kind) task for {!r}:".format(config.kind, job['label'])
        )
        yield job


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, 'worker-type', item_name=job['label'], project=config.params['project']
        )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'langpack(BM{})'.format(attributes.get('l10n_chunk', '')))
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        job['attributes'] = copy_attributes_from_dependent_job(dep_job)
        job['attributes']['chunk_locales'] = dep_job.attributes['chunk_locales']

        job['description'] = job['description'].format(
            locales='/'.join(job['attributes']['chunk_locales']),
            platform=job['attributes']['build_platform']
        )

        job['scopes'] = [
            get_beetmover_bucket_scope(config),
            get_beetmover_action_scope(config),
        ]

        job['dependencies'] = {
            str(dep_job.kind): dep_job.label
        }

        job['run-on-projects'] = dep_job.attributes['run_on_projects']
        job['treeherder'] = treeherder
        job['shipping-phase'] = dep_job.attributes['shipping_phase']
        job['shipping-product'] = dep_job.attributes['shipping_product']

        yield job


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        signing_task_ref = get_upstream_task_ref(
            job, expected_kinds=('release-sign-and-push-langpacks',)
        )

        job['worker'] = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                signing_task_ref, job['attributes']['chunk_locales']
            ),
        }

        yield job


def generate_upstream_artifacts(upstream_task_ref, locales):
    return [{
        'taskId': {'task-reference': upstream_task_ref},
        'taskType': 'scriptworker',
        'locale': locale,
        'paths': [
            # addonscript uploads en-US XPI in the en-US folder
            'public/build/{}/target.langpack.xpi'.format(locale)
        ],
    } for locale in locales]


@transforms.add
def strip_unused_data(config, jobs):
    for job in jobs:
        del job['dependent-task']

        yield job


@transforms.add
def yield_all_platform_jobs(config, jobs):
    # Even though langpacks are now platform independent, we keep beetmoving them at old
    # platform-specific locations. That's why this transform exist
    for job in jobs:
        if 'ja-JP-mac' in job['label']:
            # This locale must not be copied on any other platform than macos
            yield job
        else:
            platforms = ('linux', 'linux64', 'macosx64', 'win32', 'win64')
            if 'devedition' in job['attributes']['build_platform']:
                platforms = ('{}-devedition'.format(plat) for plat in platforms)
            for platform in platforms:
                platform_job = copy.deepcopy(job)
                if 'ja' in platform_job['attributes']['chunk_locales'] and \
                        platform in ('macosx64', 'macosx64-devedition'):
                    platform_job = _strip_ja_data_from_linux_job(platform_job)

                platform_job = _change_platform_data(platform_job, platform)

                yield platform_job


def _strip_ja_data_from_linux_job(platform_job):
    # Let's take "ja" out the description. This locale is in a substring like "aa/bb/cc/dd", where
    # "ja" could be any of "aa", "bb", "cc", "dd"
    platform_job['description'] = platform_job['description'].replace('ja/', '')
    platform_job['description'] = platform_job['description'].replace('/ja', '')

    platform_job['worker']['upstream-artifacts'] = [
        artifact
        for artifact in platform_job['worker']['upstream-artifacts']
        if artifact['locale'] != 'ja'
    ]

    return platform_job


def _change_platform_data(platform_job, platform):
    orig_platform = 'linux64'
    if 'devedition' in platform:
        orig_platform = 'linux64-devedition'
    platform_job['attributes']['build_platform'] = platform
    platform_job['label'] = platform_job['label'].replace(orig_platform, platform)
    platform_job['description'] = platform_job['description'].replace(orig_platform, platform)
    platform_job['treeherder']['platform'] = platform_job['treeherder']['platform'].replace(
        orig_platform, platform
    )
    platform_job['worker']['release-properties']['platform'] = platform

    return platform_job
