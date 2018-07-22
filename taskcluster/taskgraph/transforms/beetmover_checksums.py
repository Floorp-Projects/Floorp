# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope,
                                         get_worker_type_for_scope)
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
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('locale'): basestring,
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

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'BMcs(N)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault(
            'tier',
            dep_job.task.get('extra', {}).get('treeherder', {}).get('tier', 1)
        )
        treeherder.setdefault('kind', 'build')

        label = job['label']
        build_platform = attributes.get('build_platform')

        description = (
            "Beetmover submission of checksums for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get('locale', 'en-US'),
                build_platform=build_platform,
                build_type=attributes.get('build_type')
            )
        )

        extra = {}
        if build_platform.startswith("android"):
            extra['product'] = 'fennec'
        elif 'devedition' in build_platform:
            extra['product'] = 'devedition'
        else:
            extra['product'] = 'firefox'

        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith('beetmover'):
                dependencies[k] = v

        attributes = copy_attributes_from_dependent_job(dep_job)

        if dep_job.attributes.get('locale'):
            treeherder['symbol'] = 'BMcs({})'.format(dep_job.attributes.get('locale'))
            attributes['locale'] = dep_job.attributes.get('locale')

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

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
        "public/target.checksums",
        "public/target.checksums.asc",
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

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
            'upstream-artifacts': generate_upstream_artifacts(
                refs, platform, locale
            ),
        }

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
