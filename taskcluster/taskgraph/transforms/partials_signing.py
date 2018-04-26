# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the partials task into an actual task description.
"""
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (
    get_signing_cert_scope_per_platform,
    get_worker_type_for_scope,
)
from taskgraph.util.partials import get_balrog_platform_name, get_partials_artifacts
from taskgraph.util.taskcluster import get_artifact_prefix

import logging
logger = logging.getLogger(__name__)

transforms = TransformSequence()


def generate_upstream_artifacts(job, release_history, platform, locale=None):
    artifact_prefix = get_artifact_prefix(job)
    if locale:
        artifact_prefix = '{}/{}'.format(artifact_prefix, locale)
    else:
        locale = 'en-US'

    artifacts = get_partials_artifacts(release_history, platform, locale)

    upstream_artifacts = [{
        "taskId": {"task-reference": '<partials>'},
        "taskType": 'partials',
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in artifacts],
        "formats": ["mar_sha384"],
    }]

    return upstream_artifacts


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'ps(N)')

        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        label = job.get('label', "partials-signing-{}".format(dep_job.label))
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
        locale = dep_job.attributes.get('locale')
        if locale:
            attributes['locale'] = locale
            treeherder['symbol'] = 'ps({})'.format(locale)

        balrog_platform = get_balrog_platform_name(dep_th_platform)
        upstream_artifacts = generate_upstream_artifacts(
            dep_job, config.params['release_history'], balrog_platform, locale)

        build_platform = dep_job.attributes.get('build_platform')
        is_nightly = dep_job.attributes.get('nightly')
        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_nightly, config
        )
        scopes = [signing_cert_scope, 'project:releng:signing:format:mar_sha384']
        task = {
            'label': label,
            'description': "{} Partials".format(
                dep_job.task["metadata"]["description"]),
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
