# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the {partials,mar}-signing task into an actual task description.
"""
from __future__ import absolute_import, print_function, unicode_literals

import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job, sorted_unique_list
from taskgraph.util.scriptworker import (
    get_signing_cert_scope_per_platform,
    get_worker_type_for_scope,
    get_autograph_format_scope,
)
from taskgraph.util.partials import get_balrog_platform_name, get_partials_artifacts
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.treeherder import join_symbol

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


def generate_partials_artifacts(job, release_history, platform, locale=None):
    artifact_prefix = get_artifact_prefix(job)
    if locale:
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)
    else:
        locale = 'en-US'

    artifacts = get_partials_artifacts(release_history, platform, locale)

    upstream_artifacts = [{
        "taskId": {"task-reference": '<partials>'},
        "taskType": 'partials',
        "paths": [
            "{}/{}".format(artifact_prefix, path)
            for path, version in artifacts
            # TODO Use mozilla-version to avoid comparing strings. Otherwise Firefox 100 will be
            # considered smaller than Firefox 56
            if version is None or version >= '56'
        ],
        "formats": ["autograph_hash_only_mar384"],
    }]

    old_mar_upstream_artifacts = {
        "taskId": {"task-reference": '<partials>'},
        "taskType": 'partials',
        "paths": [
            "{}/{}".format(artifact_prefix, path)
            for path, version in artifacts
            # TODO Use mozilla-version to avoid comparing strings. Otherwise Firefox 100 will be
            # considered smaller than Firefox 56
            if version is not None and version < '56'
        ],
        "formats": ["mar"],
    }

    if old_mar_upstream_artifacts["paths"]:
        upstream_artifacts.append(old_mar_upstream_artifacts)

    return upstream_artifacts


def generate_complete_artifacts(job):
    upstream_artifacts = []
    for artifact in job.release_artifacts:
        basename = os.path.basename(artifact)
        if basename.endswith('.complete.mar'):
            upstream_artifacts.append({
                "taskId": {"task-reference": '<{}>'.format(job.kind)},
                "taskType": 'build',
                "paths": [artifact],
                "formats": ["autograph_hash_only_mar384"],
            })

    return upstream_artifacts


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        locale = dep_job.attributes.get('locale')

        treeherder = job.get('treeherder', {})
        treeherder['symbol'] = join_symbol(
            job.get('treeherder-group', 'ms'),
            locale or 'N'
        )

        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        label = job.get('label', "{}-{}".format(config.kind, dep_job.label))
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('kind', 'build')
        treeherder.setdefault('tier', 1)

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}
        signing_dependencies = dep_job.dependencies
        # This is so we get the build task etc in our dependencies to
        # have better beetmover support.
        dependencies.update(signing_dependencies)

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['required_signoffs'] = sorted_unique_list(
            attributes.get('required_signoffs', []),
            job.pop('required_signoffs')
        )
        attributes['shipping_phase'] = job['shipping-phase']
        if locale:
            attributes['locale'] = locale

        balrog_platform = get_balrog_platform_name(dep_th_platform)
        if config.kind == 'partials-signing':
            upstream_artifacts = generate_partials_artifacts(
                dep_job, config.params['release_history'], balrog_platform, locale)
        else:
            upstream_artifacts = generate_complete_artifacts(dep_job)

        build_platform = dep_job.attributes.get('build_platform')
        is_nightly = dep_job.attributes.get('nightly')
        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_nightly, config
        )
        autograph_hash_format_scope = get_autograph_format_scope(config)

        scopes = [signing_cert_scope, autograph_hash_format_scope]
        if any("mar" in upstream_details["formats"] for upstream_details in upstream_artifacts):
            scopes.append('project:releng:signing:format:mar')

        task = {
            'label': label,
            'description': "{} {}".format(
                dep_job.task["metadata"]["description"], job['description-suffix']),
            'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker': {'implementation': 'scriptworker-signing',
                       'upstream-artifacts': upstream_artifacts,
                       'max-run-time': 3600},
            'dependencies': dependencies,
            'attributes': attributes,
            'scopes': scopes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task
