# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the `release-generate-checksums-beetmover` task to also append `build` as dependency
"""
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema, resolve_keyed_by, optionally_keyed_by
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_phase)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

transforms = TransformSequence()

CHECKSUMS_BUILD_ARTIFACTS = [
    "SHA256SUMMARY",
    "SHA512SUMMARY"
]

CHECKSUMS_SIGNING_ARTIFACTS = [
    "KEY",
    "SHA256SUMS",
    "SHA256SUMS.asc",
    "SHA512SUMS",
    "SHA512SUMS.asc"
]


# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

release_generate_checksums_beetmover_schema = Schema({
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

    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Required('worker-type'): optionally_keyed_by('project', basestring),
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            release_generate_checksums_beetmover_schema, job,
            "In ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        attributes = copy_attributes_from_dependent_job(dep_job)

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'BM-SGenChcks')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        job_template = "{}".format(dep_job.label)
        label = job_template.replace("signing", "beetmover")

        description = "Transfer *SUMS and *SUMMARY checksums file to S3."

        # first dependency is the signing task for the *SUMS files
        dependencies = {
            str(dep_job.kind): dep_job.label
        }

        if len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't beetmove a signing task with multiple dependencies")
        # update the dependencies with the dependencies of the signing task
        dependencies.update(dep_job.dependencies)

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)
        phase = get_phase(config)

        resolve_keyed_by(
            job, 'worker-type', item_name=label, project=config.params['project']
        )

        task = {
            'label': label,
            'description': description,
            'worker-type': job['worker-type'],
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'shipping-phase': phase,
        }

        yield task


def generate_upstream_artifacts(job, signing_task_ref, build_task_ref):
    build_mapping = CHECKSUMS_BUILD_ARTIFACTS
    signing_mapping = CHECKSUMS_SIGNING_ARTIFACTS

    artifact_prefix = get_artifact_prefix(job)

    upstream_artifacts = [{
        "taskId": {"task-reference": build_task_ref},
        "taskType": "build",
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in build_mapping],
        "locale": "en-US",
        }, {
        "taskId": {"task-reference": signing_task_ref},
        "taskType": "signing",
        "paths": ["{}/{}".format(artifact_prefix, p)
                  for p in signing_mapping],
        "locale": "en-US",
    }]

    return upstream_artifacts


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 2 and
                               any(['signing' in j for j in job['dependencies']]))
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover must have two dependencies.")

        build_task = None
        signing_task = None
        for dependency in job["dependencies"].keys():
            if 'signing' in dependency:
                signing_task = dependency
            else:
                build_task = dependency

        signing_task_ref = "<{}>".format(str(signing_task))
        build_task_ref = "<{}>".format(str(build_task))

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                job, signing_task_ref, build_task_ref
            )
        }

        job["worker"] = worker

        yield job
