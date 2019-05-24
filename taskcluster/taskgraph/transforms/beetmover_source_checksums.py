# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform release-beetmover-source-checksums into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (generate_beetmover_artifact_map,
                                         generate_beetmover_upstream_artifacts,
                                         get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_worker_type_for_scope,
                                         should_use_artifact_map)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

beetmover_checksums_description_schema = schema.extend({
    Required('depname', default='build'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('locale'): basestring,
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('attributes'): task_description_schema['attributes'],
})

transforms = TransformSequence()
transforms.add_validate(beetmover_checksums_description_schema)


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'BMcss(N)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        label = job['label']
        build_platform = attributes.get('build_platform')

        description = "Beetmover submission of checksums for source file"

        extra = {}
        if build_platform.startswith("android"):
            extra['product'] = 'fennec'
        elif 'devedition' in build_platform:
            extra['product'] = 'devedition'
        else:
            extra['product'] = 'firefox'

        dependencies = {dep_job.kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith('beetmover'):
                dependencies[k] = v

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get('attributes', {}))

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
            'extra': extra,
        }

        if 'shipping-phase' in job:
            task['shipping-phase'] = job['shipping-phase']

        if 'shipping-product' in job:
            task['shipping-product'] = job['shipping-product']

        yield task


def generate_upstream_artifacts(refs, platform, locale=None):
    # Until bug 1331141 is fixed, if you are adding any new artifacts here that
    # need to be transfered to S3, please be aware you also need to follow-up
    # with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
    # See example in bug 1348286
    common_paths = [
        "public/target-source.checksums",
        "public/target-source.checksums.asc",
    ]

    upstream_artifacts = [{
        "taskId": {"task-reference": refs["signing"]},
        "taskType": "signing",
        "paths": common_paths,
        "locale": locale or "en-US",
    }]

    return upstream_artifacts


@transforms.add
def make_beetmover_checksums_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = (len(job["dependencies"]) == 2)
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover checksums must have two dependencies.")

        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        refs = {
            "beetmover": None,
            "signing": None,
        }
        for dependency in job["dependencies"].keys():
            if dependency.startswith("beetmover"):
                refs['beetmover'] = "<{}>".format(dependency)
            else:
                refs['signing'] = "<{}>".format(dependency)
        if None in refs.values():
            raise NotImplementedError(
                "Beetmover checksums must have a beetmover and signing dependency!")

        if should_use_artifact_map(platform):
            upstream_artifacts = generate_beetmover_upstream_artifacts(config,
                                                                       job, platform, locale)
        else:
            upstream_artifacts = generate_upstream_artifacts(refs, platform, locale)

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': upstream_artifacts,
        }

        if should_use_artifact_map(platform):
            worker['artifact-map'] = generate_beetmover_artifact_map(
                config, job, platform=platform)

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
