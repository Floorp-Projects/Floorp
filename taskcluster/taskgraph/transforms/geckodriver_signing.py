# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_signing_cert_scope_per_platform,
    get_worker_type_for_scope,
)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

repackage_signing_description_schema = schema.extend({
    Required('depname', default='geckodriver-repackage'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
})

transforms = TransformSequence()
transforms.add_validate(repackage_signing_description_schema)


@transforms.add
def make_repackage_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['repackage_type'] = 'repackage-signing'

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'Gd(s)')
        treeherder.setdefault('platform', dep_job.task.get('extra', {}).get('treeherder-platform'))
        treeherder.setdefault(
            'tier',
            dep_job.task.get('extra', {}).get('treeherder', {}).get('tier', 1)
        )
        treeherder.setdefault('kind', 'build')

        dependencies = {dep_job.kind: dep_job.label}
        signing_dependencies = dep_job.dependencies
        dependencies.update({
            k: v for k, v in signing_dependencies.items()
            if k != 'docker-image'
        })

        description = "Signing Geckodriver for build '{}/{}'".format(
            attributes.get('build_platform'),
            attributes.get('build_type'),
        )

        build_platform = dep_job.attributes.get('build_platform')
        is_nightly = dep_job.attributes.get('nightly', dep_job.attributes.get('shippable'))
        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_nightly, config
        )

        upstream_artifacts = _craft_upstream_artifacts(dep_job.kind, build_platform)

        scopes = [signing_cert_scope]
        scopes += list({
            add_scope_prefix(config, 'signing:format:{}'.format(format))
            for artifact in upstream_artifacts
            for format in artifact['formats']
        })

        task = {
            'label': job['label'],
            'description': description,
            'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker': {
                'implementation': 'scriptworker-signing',
                'upstream-artifacts': upstream_artifacts,
            },
            'scopes': scopes,
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task


def _craft_upstream_artifacts(dependency_kind, build_platform):
    if build_platform.startswith('win'):
        signing_format = 'sha2signcode'
        extension = 'zip'
    elif build_platform.startswith('linux'):
        signing_format = 'autograph_gpg'
        extension = 'tar.gz'
    else:
        raise ValueError('Unsupported build platform "{}"'.format(build_platform))

    return [{
        'taskId': {'task-reference': '<{}>'.format(dependency_kind)},
        'taskType': 'repackage',
        'paths': ['public/geckodriver.{}'.format(extension)],
        'formats': [signing_format],
    }]
