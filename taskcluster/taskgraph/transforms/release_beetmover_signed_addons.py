# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         generate_beetmover_upstream_artifacts,
                                         generate_beetmover_artifact_map,
                                         should_use_artifact_map)
from taskgraph.util.treeherder import inherit_treeherder_from_dep
from taskgraph.transforms.task import task_description_schema
from taskgraph.transforms.release_sign_and_push_langpacks import get_upstream_task_ref
from voluptuous import Required, Optional

import logging
import copy

logger = logging.getLogger(__name__)


transforms = TransformSequence()


beetmover_description_schema = schema.extend({
    # depname is used in taskref's to identify the taskID of the unsigned things
    Required('depname', default='build'): basestring,

    # attributes is used for enabling artifact-map by declarative artifacts
    Required('attributes'): {basestring: object},

    # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
    Optional('label'): basestring,

    # treeherder is allowed here to override any defaults we use for beetmover.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],

    Required('description'): basestring,
    Required('worker-type'): optionally_keyed_by('release-level', basestring),
    Required('run-on-projects'): [],

    # locale is passed only for l10n beetmoving
    Optional('locale'): basestring,
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def set_label(config, jobs):
    for job in jobs:
        job['label'] = job['primary-dependency'].label.replace(
            'push-langpacks', 'beetmover-signed-langpacks'
        )

        yield job


transforms.add_validate(beetmover_description_schema)


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        for field in ('worker-type', 'attributes.artifact_map'):
            resolve_keyed_by(
                job, field, item_name=job['label'],
                **{
                    'release-level': config.params.release_level(),
                    'project': config.params['project']
                }
            )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        treeherder.setdefault('symbol', 'langpack(BM{})'.format(attributes.get('l10n_chunk', '')))

        job['attributes'].update(copy_attributes_from_dependent_job(dep_job))
        job['attributes']['chunk_locales'] = dep_job.attributes['chunk_locales']

        job['description'] = job['description'].format(
            locales='/'.join(job['attributes']['chunk_locales']),
            platform=job['attributes']['build_platform']
        )

        job['scopes'] = [
            get_beetmover_bucket_scope(config),
            get_beetmover_action_scope(config),
        ]

        job['dependencies'] = {dep_job.kind: dep_job.label}

        job['run-on-projects'] = dep_job.attributes['run_on_projects']
        job['treeherder'] = treeherder
        job['shipping-phase'] = dep_job.attributes['shipping_phase']
        job['shipping-product'] = dep_job.attributes['shipping_product']

        yield job


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        signing_task_ref = get_upstream_task_ref(
            job, expected_kinds=('release-push-langpacks',)
        )

        platform = job["attributes"]["build_platform"]
        locale = job["attributes"]["chunk_locales"]
        if should_use_artifact_map(platform):
            upstream_artifacts = generate_beetmover_upstream_artifacts(
                config, job, platform, locale,
            )
        else:
            upstream_artifacts = generate_upstream_artifacts(
                signing_task_ref, job['attributes']['chunk_locales']
            )
        job['worker'] = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': upstream_artifacts,
        }

        if should_use_artifact_map(platform):
            job['worker']['artifact-map'] = generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale)

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
        del job['primary-dependency']

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

                platform_job = _change_platform_data(config, platform_job, platform)

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


def _change_platform_in_artifact_map_paths(paths, orig_platform, new_platform):
    amended_paths = {}
    for artifact, artifact_info in paths.iteritems():
        amended_artifact_info = {
            'checksums_path': artifact_info['checksums_path'].replace(orig_platform, new_platform),
            'destinations': [
                d.replace(orig_platform, new_platform) for d in artifact_info['destinations']
            ]
        }
        amended_paths[artifact] = amended_artifact_info

    return amended_paths


def _change_platform_data(config, platform_job, platform):
    orig_platform = 'linux64'
    if 'devedition' in platform:
        orig_platform = 'linux64-devedition'
    backup_platform = platform_job['attributes']['build_platform']
    platform_job['attributes']['build_platform'] = platform
    platform_job['label'] = platform_job['label'].replace(orig_platform, platform)
    platform_job['description'] = platform_job['description'].replace(orig_platform, platform)
    platform_job['treeherder']['platform'] = platform_job['treeherder']['platform'].replace(
        orig_platform, platform
    )
    platform_job['worker']['release-properties']['platform'] = platform

    # amend artifactMap entries as well
    if should_use_artifact_map(backup_platform):
        platform_mapping = {
            'linux64': 'linux-x86_64',
            'linux': 'linux-i686',
            'macosx64': 'mac',
            'win32': 'win32',
            'win64': 'win64',
        }
        orig_platform = platform_mapping.get(orig_platform, orig_platform)
        platform = platform_mapping.get(platform, platform)
        platform_job['worker']['artifact-map'] = [
            {
                'locale': entry['locale'],
                'taskId': entry['taskId'],
                'paths': _change_platform_in_artifact_map_paths(entry['paths'],
                                                                orig_platform,
                                                                platform)
            } for entry in platform_job['worker']['artifact-map']
        ]

    return platform_job
