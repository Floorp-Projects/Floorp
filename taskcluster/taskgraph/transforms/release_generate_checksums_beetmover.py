# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the `release-generate-checksums-beetmover` task to also append `build` as dependency
"""
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (generate_beetmover_artifact_map,
                                         generate_beetmover_upstream_artifacts,
                                         get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_worker_type_for_scope,
                                         should_use_artifact_map,
                                         )
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


release_generate_checksums_beetmover_schema = schema.extend({
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

    Optional('attributes'): task_description_schema['attributes'],
})

transforms = TransformSequence()
transforms.add_validate(release_generate_checksums_beetmover_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get('attributes', {}))

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
        dependencies = {dep_job.kind: dep_job.label}

        if len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't beetmove a signing task with multiple dependencies")
        # update the dependencies with the dependencies of the signing task
        dependencies.update(dep_job.dependencies)

        bucket_scope = get_beetmover_bucket_scope(
            config, job_release_type=attributes.get('release-type')
        )
        action_scope = get_beetmover_action_scope(
            config, job_release_type=attributes.get('release-type')
        )

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, bucket_scope),
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'shipping-phase': 'promote',
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
        }

        platform = job["attributes"]["build_platform"]
        # Works with Firefox/Devedition. Commented for migration.
        if should_use_artifact_map(platform):
            upstream_artifacts = generate_beetmover_upstream_artifacts(
                config, job, platform=None, locale=None
            )
        else:
            upstream_artifacts = generate_upstream_artifacts(
                job, signing_task_ref, build_task_ref
            )

        worker['upstream-artifacts'] = upstream_artifacts

        # Works with Firefox/Devedition. Commented for migration.
        if should_use_artifact_map(platform):
            worker['artifact-map'] = generate_beetmover_artifact_map(
                config, job, platform=platform)

        job["worker"] = worker

        yield job
