# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (get_balrog_server_scope,
                                         get_balrog_channel_scopes)
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional


# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

# shortcut for a string where task references are allowed
taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

balrog_description_schema = Schema({
    # the dependent task (object) for this balrog job, used to inform balrogworker.
    Required('dependent-task'): object,

    # unique label to describe this balrog task, defaults to balrog-{dep.label}
    Optional('label'): basestring,

    # treeherder is allowed here to override any defaults we use for beetmover.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        yield validate_schema(
            balrog_description_schema, job,
            "In balrog ({!r} kind) task for {!r}:".format(config.kind, label))


@transforms.add
def skip_unsigned_beets(config, jobs):
    for job in jobs:
        if ('signing' not in job['dependent-task'].label and
                'beetmover-repackage' not in job['dependent-task'].label):
            # Skip making a balrog task for this
            continue
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'tc-Up(N)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        attributes = {
            'nightly': dep_job.attributes.get('nightly', False),
            'build_platform': dep_job.attributes.get('build_platform'),
            'build_type': dep_job.attributes.get('build_type'),
        }

        if dep_job.attributes.get('locale'):
            treeherder['symbol'] = 'tc-Up({})'.format(dep_job.attributes.get('locale'))
            attributes['locale'] = dep_job.attributes.get('locale')

        label = job.get('label', "balrog-{}".format(dep_job.label))

        upstream_artifacts = [{
            "taskId": {"task-reference": "<beetmover>"},
            "taskType": "beetmover",
            "paths": [
                "public/manifest.json"
            ],
        }]

        server_scope = get_balrog_server_scope(config)
        channel_scopes = get_balrog_channel_scopes(config)

        task = {
            'label': label,
            'description': "{} Balrog".format(
                dep_job.task["metadata"]["description"]),
            # do we have to define worker type somewhere?
            'worker-type': 'scriptworker-prov-v1/balrogworker-v1',
            'worker': {
                'implementation': 'balrog',
                'upstream-artifacts': upstream_artifacts,
            },
            'scopes': [server_scope] + channel_scopes,
            'dependencies': {'beetmover': dep_job.label},
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        yield task
