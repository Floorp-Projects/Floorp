# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import get_signing_cert_scope
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

checksums_signing_description_schema = Schema({
    Required('dependent-task'): object,
    Required('depname', default='beetmover'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        yield validate_schema(
            checksums_signing_description_schema, job,
            "In checksums-signing ({!r} kind) task for {!r}:".format(config.kind, label))


@transforms.add
def make_checksums_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'tc-cs(N)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        label = job.get('label', "checksumssigning-{}".format(dep_job.label))
        dependencies = {"beetmover": dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)

        if dep_job.attributes.get('locale'):
            treeherder['symbol'] = 'tc-cs({})'.format(dep_job.attributes.get('locale'))
            attributes['locale'] = dep_job.attributes.get('locale')

        upstream_artifacts = [{
            "taskId": {"task-reference": "<beetmover>"},
            "taskType": "beetmover",
            "paths": [
                "public/target.checksums",
            ],
            "formats": ["gpg"]
        }]

        signing_cert_scope = get_signing_cert_scope(config)
        task = {
            'label': label,
            'description': "Checksum signing {} ".format(
                dep_job.task["metadata"]["description"]),
            'worker-type': "scriptworker-prov-v1/signing-linux-v1",
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
