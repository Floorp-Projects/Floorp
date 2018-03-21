# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-generate-checksums-signing task into task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (
    get_signing_cert_scope,
    get_worker_type_for_scope,
)
from taskgraph.util.taskcluster import get_artifact_path
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

release_generate_checksums_signing_schema = Schema({
    Required('dependent-task'): object,
    Required('depname', default='release-generate-checksums'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            release_generate_checksums_signing_schema, job,
            "In ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_release_generate_checksums_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
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
            "build": dep_job.label
        }

        upstream_artifacts = [{
            "taskId": {"task-reference": "<build>"},
            "taskType": "build",
            "paths": [
                get_artifact_path(dep_job, "SHA256SUMS"),
                get_artifact_path(dep_job, "SHA512SUMS"),
            ],
            "formats": ["gpg"]
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
                "project:releng:signing:format:gpg"
            ],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task
