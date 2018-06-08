# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform release-beetmover-source-checksums into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

beetmover_checksums_description_schema = Schema({
    Required('dependent-task'): object,
    Required('depname', default='build'): basestring,
    Optional('label'): basestring,
    Optional('extra'): object,
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            beetmover_checksums_description_schema, job,
            "In checksums-signing ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        attributes = dep_job.attributes
        build_platform = attributes.get("build_platform")
        if not build_platform:
            raise Exception("Cannot find build platform!")
        repack_id = dep_job.task.get('extra', {}).get('repack_id')
        if not repack_id:
            raise Exception("Cannot find repack id!")

        label = dep_job.label.replace("beetmover-", "beetmover-checksums-")
        description = (
            "Beetmove checksums for repack_id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=repack_id,
                build_platform=build_platform,
                build_type=attributes.get('build_type')
            )
        )

        extra = {}
        extra['partner_path'] = dep_job.task['payload']['upstreamArtifacts'][0]['locale']
        extra['repack_id'] = repack_id

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith('beetmover'):
                dependencies[k] = v

        attributes = copy_attributes_from_dependent_job(dep_job)

        task = {
            'label': label,
            'description': description,
            'worker-type': '{}/{}'.format(
                dep_job.task['provisionerId'],
                dep_job.task['workerType'],
            ),
            'scopes': dep_job.task['scopes'],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'extra': extra,
        }

        if 'shipping-phase' in job:
            task['shipping-phase'] = job['shipping-phase']

        if 'shipping-product' in job:
            task['shipping-product'] = job['shipping-product']

        yield task


def generate_upstream_artifacts(refs, partner_path):
    # Until bug 1331141 is fixed, if you are adding any new artifacts here that
    # need to be transfered to S3, please be aware you also need to follow-up
    # with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
    # See example in bug 1348286
    common_paths = [
        "public/target.checksums",
    ]

    upstream_artifacts = [{
        "taskId": {"task-reference": refs["beetmover"]},
        "taskType": "signing",
        "paths": common_paths,
        "locale": "beetmover-checksums/{}".format(partner_path),
    }]

    return upstream_artifacts


@transforms.add
def make_beetmover_checksums_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 1)
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover checksums must have one dependency.")

        refs = {
            "beetmover": None,
        }
        for dependency in job["dependencies"].keys():
            if dependency.endswith("beetmover"):
                refs['beetmover'] = "<{}>".format(dependency)
        if None in refs.values():
            raise NotImplementedError(
                "Beetmover checksums must have a beetmover dependency!")

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                refs, job['extra']['partner_path'],
            ),
            'partner-public': True,
        }

        job["worker"] = worker

        yield job
