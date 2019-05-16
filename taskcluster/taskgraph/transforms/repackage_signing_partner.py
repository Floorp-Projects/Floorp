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
from taskgraph.util.partners import check_if_partners_enabled
from taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_signing_cert_scope_per_platform,
    get_worker_type_for_scope,
)
from taskgraph.util.taskcluster import get_artifact_path
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

transforms = TransformSequence()

repackage_signing_description_schema = schema.extend({
    Required('depname', default='repackage'): basestring,
    Optional('label'): basestring,
    Optional('extra'): object,
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
})

transforms.add(check_if_partners_enabled)
transforms.add_validate(repackage_signing_description_schema)


@transforms.add
def make_repackage_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        repack_id = dep_job.task['extra']['repack_id']
        attributes = dep_job.attributes
        build_platform = dep_job.attributes.get('build_platform')
        is_nightly = dep_job.attributes.get('nightly', dep_job.attributes.get('shippable'))

        # Mac & windows
        label = dep_job.label.replace("repackage-", "repackage-signing-")
        # Linux
        label = label.replace("chunking-dummy-", "repackage-signing-")
        description = (
            "Signing of repackaged artifacts for partner repack id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=repack_id,
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        if 'linux' in build_platform:
            # we want the repack job, via the dependencies for the the chunking-dummy dep_job
            for dep in dep_job.dependencies.values():
                if dep.startswith('release-partner-repack'):
                    dependencies = {"repack": dep}
                    break
        else:
            # we have a genuine repackage job as our parent
            dependencies = {"repackage": dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['repackage_type'] = 'repackage-signing'

        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_nightly, config
        )
        scopes = [signing_cert_scope]

        if 'win' in build_platform:
            upstream_artifacts = [{
                "taskId": {"task-reference": "<repackage>"},
                "taskType": "repackage",
                "paths": [
                    get_artifact_path(dep_job, "{}/target.installer.exe".format(repack_id)),
                ],
                "formats": ["sha2signcode", "autograph_gpg"]
            }]
            scopes.append(add_scope_prefix(config, "signing:format:sha2signcode"))
        elif 'mac' in build_platform:
            upstream_artifacts = [{
                "taskId": {"task-reference": "<repackage>"},
                "taskType": "repackage",
                "paths": [
                    get_artifact_path(dep_job, "{}/target.dmg".format(repack_id)),
                ],
                "formats": ["autograph_gpg"]
            }]
        elif 'linux' in build_platform:
            upstream_artifacts = [{
                "taskId": {"task-reference": "<repack>"},
                "taskType": "repackage",
                "paths": [
                    get_artifact_path(dep_job, "{}/target.tar.bz2".format(repack_id)),
                ],
                "formats": ["autograph_gpg"]
            }]

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker': {'implementation': 'scriptworker-signing',
                       'upstream-artifacts': upstream_artifacts,
                       'max-run-time': 3600},
            'scopes': scopes,
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'extra': {
                'repack_id': repack_id,
            }
        }

        yield task
