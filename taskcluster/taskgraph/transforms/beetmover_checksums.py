# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import validate_schema
from taskgraph.util.scriptworker import (get_beetmover_bucket_scope,
                                         get_beetmover_action_scope)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Schema, Any, Required, Optional

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
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        yield validate_schema(
            beetmover_checksums_description_schema, job,
            "In checksums-signing ({!r} kind) task for {!r}:".format(config.kind, label))


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'tc-BMcs(N)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        label = job.get('label', "beetmover-{}".format(dep_job.label))
        dependent_kind = str(dep_job.kind)
        dependencies = {dependent_kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith('beetmover'):
                dependencies[k] = v

        attributes = {
            'nightly': dep_job.attributes.get('nightly', False),
            'build_platform': dep_job.attributes.get('build_platform'),
            'build_type': dep_job.attributes.get('build_type'),
        }
        if dep_job.attributes.get('locale'):
            treeherder['symbol'] = 'tc-BMcs({})'.format(dep_job.attributes.get('locale'))
            attributes['locale'] = dep_job.attributes.get('locale')

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            'label': label,
            'description': "Beetmover {} ".format(
                dep_job.task["metadata"]["description"]),
            'worker-type': 'scriptworker-prov-v1/beetmoverworker-v1',
            'scopes': [bucket_scope, action_scope],
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task


def generate_upstream_artifacts(refs, platform, locale=None):
    common_paths = [
        "public/target.checksums",
        "public/target.checksums.asc",
    ]

    upstream_artifacts = [{
        "taskId": {"task-reference": refs["signing"]},
        "taskType": "signing",
        "paths": common_paths,
        "locale": locale or "en-US",
    }, {
        "taskId": {"task-reference": refs["beetmover"]},
        "taskType": "beetmover",
        "paths": ["public/balrog_props.json"],
        "locale": locale or "en-US",
    }]

    if not locale and "android" in platform:
        # edge case to support 'multi' locale paths
        upstream_artifacts.extend([{
            "taskId": {"task-reference": refs["signing"]},
            "taskType": "signing",
            "paths": common_paths,
            "locale": "multi"
        }])

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

        upstream_artifacts = generate_upstream_artifacts(refs,
                                                         platform, locale)

        worker = {'implementation': 'beetmover',
                  'upstream-artifacts': upstream_artifacts}
        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
