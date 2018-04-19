# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.partners import check_if_partners_enabled
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_signing_cert_scope_per_platform,
)
from taskgraph.util.taskcluster import get_artifact_path
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

repackage_signing_description_schema = Schema({
    Required('dependent-task'): object,
    Required('depname', default='repackage'): basestring,
    Optional('label'): basestring,
    Optional('extra'): object,
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
})

transforms.add(check_if_partners_enabled)


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            repackage_signing_description_schema, job,
            "In repackage-signing ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_repackage_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        repack_id = dep_job.task['extra']['repack_id']
        attributes = dep_job.attributes

        label = dep_job.label.replace("repackage-", "repackage-signing-")
        description = (
            "Signing of repackaged artifacts for partner repack id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=repack_id,
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        dependencies = {"repackage": dep_job.label}

        signing_dependencies = dep_job.dependencies
        # This is so we get the build task etc in our dependencies to
        # have better beetmover support.
        dependencies.update({k: v for k, v in signing_dependencies.items()
                             if k != 'docker-image'})
        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['repackage_type'] = 'repackage-signing'

        build_platform = dep_job.attributes.get('build_platform')
        is_nightly = dep_job.attributes.get('nightly')
        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_nightly, config
        )
        scopes = [signing_cert_scope]

        if 'win' not in build_platform:
            raise Exception("Repackage signing is not supported for non-Windows partner repacks.")

        upstream_artifacts = [{
            "taskId": {"task-reference": "<repackage>"},
            "taskType": "repackage",
            "paths": [
                get_artifact_path(dep_job, "{}/target.installer.exe".format(repack_id)),
            ],
            "formats": ["sha2signcode"]
        }]
        scopes.append(add_scope_prefix(config, "signing:format:sha2signcode"))

        task = {
            'label': label,
            'description': description,
            # 'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker-type': 'scriptworker-prov-v1/signing-linux-v1',
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
