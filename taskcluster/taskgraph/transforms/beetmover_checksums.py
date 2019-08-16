# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals


from taskgraph.loader.single_dep import schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.beetmover import craft_release_properties
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.scriptworker import (generate_beetmover_artifact_map,
                                         generate_beetmover_upstream_artifacts,
                                         get_beetmover_action_scope,
                                         get_beetmover_bucket_scope,
                                         get_worker_type_for_scope,
                                         should_use_artifact_map)
from voluptuous import Optional, Required
from taskgraph.util.treeherder import replace_group
from taskgraph.transforms.task import task_description_schema

beetmover_checksums_description_schema = schema.extend({
    Required('depname', default='build'): basestring,
    Required('attributes'): {basestring: object},
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('locale'): basestring,
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
})

transforms = TransformSequence()
transforms.add_validate(beetmover_checksums_description_schema)


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = job['primary-dependency']
        attributes = dep_job.attributes

        treeherder = job.get('treeherder', {})
        treeherder.setdefault(
            'symbol',
            replace_group(dep_job.task['extra']['treeherder']['symbol'], 'BMcs')
        )
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

        dependencies = {dep_job.kind: dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get('attributes', {}))

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


def generate_upstream_artifacts(locale=None):
    # XXX: this function is called solely for Devedition, until we fix 1537713
    # so that DevEdition uses in-tree manifests too. This will be fixed in Q3
    upstream_artifacts = [{
        "taskId": {"task-reference": "<beetmover-repackage>"},
        "taskType": "beetmover",
        "paths": [
            "public/target.checksums",
        ],
        "locale": locale or "en-US",
    }]

    return upstream_artifacts


@transforms.add
def make_beetmover_checksums_worker(config, jobs):
    for job in jobs:
        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        worker = {
            'implementation': 'beetmover',
            'release-properties': craft_release_properties(config, job),
        }

        if should_use_artifact_map(platform):
            upstream_artifacts = generate_beetmover_upstream_artifacts(
                config, job, platform, locale
            )
            worker['artifact-map'] = generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale)
        else:
            upstream_artifacts = generate_upstream_artifacts(
                locale
            )
            # Clean up un-used artifact map, to avoid confusion
            if job['attributes'].get('artifact_map'):
                del job['attributes']['artifact_map']

        worker['upstream-artifacts'] = upstream_artifacts

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
