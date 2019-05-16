# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-generate-checksums-signing task into task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (
    get_signing_cert_scope,
    get_worker_type_for_scope,
)
from taskgraph.util.taskcluster import get_artifact_path
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

release_generate_checksums_signing_schema = schema.extend({
    Required('depname', default='release-generate-checksums'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
})

transforms = TransformSequence()
transforms.add_validate(release_generate_checksums_signing_schema)


@transforms.add
def make_release_generate_checksums_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = copy_attributes_from_dependent_job(dep_job)

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'SGenChcks')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        job_template = "{}-{}".format(dep_job.label, "signing")
        label = job.get("label", job_template)
        description = "Signing of the overall release-related checksums"

        dependencies = {
            str(dep_job.kind): dep_job.label
        }

        upstream_artifacts = [{
            "taskId": {"task-reference": "<{}>".format(str(dep_job.kind))},
            "taskType": "build",
            "paths": [
                get_artifact_path(dep_job, "SHA256SUMS"),
                get_artifact_path(dep_job, "SHA512SUMS"),
            ],
            "formats": ["autograph_gpg"]
        }]

        signing_cert_scope = get_signing_cert_scope(config)

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker': {'implementation': 'scriptworker-signing',
                       'upstream-artifacts': upstream_artifacts,
                       'max-run-time': 3600},
            'scopes': [
                signing_cert_scope,
            ],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task
