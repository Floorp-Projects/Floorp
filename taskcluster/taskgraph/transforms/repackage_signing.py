# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import get_signing_cert_scope
from taskgraph.transforms.task import task_description_schema
from voluptuous import Required, Optional

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

transforms = TransformSequence()

repackage_signing_description_schema = Schema({
    Required('dependent-task'): object,
    Required('depname', default='repackage'): basestring,
    Optional('label'): basestring,
    Optional('treeherder'): task_description_schema['treeherder'],
})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        yield validate_schema(
            repackage_signing_description_schema, job,
            "In repackage-signing ({!r} kind) task for {!r}:".format(config.kind, label))


@transforms.add
def make_repackage_signing_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'tc-rs(N)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform',
                              "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')

        label = job.get('label', "repackage-signing-{}".format(dep_job.label))
        dependencies = {"repackage": dep_job.label}

        signing_dependencies = dep_job.dependencies
        # This is so we get the build task etc in our dependencies to
        # have better beetmover support.
        dependencies.update(signing_dependencies)
        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['repackage_type'] = 'repackage-signing'

        locale_str = ""
        if dep_job.attributes.get('locale'):
            treeherder['symbol'] = 'tc-rs({})'.format(dep_job.attributes.get('locale'))
            attributes['locale'] = dep_job.attributes.get('locale')
            locale_str = "{}/".format(dep_job.attributes.get('locale'))

        scopes = [get_signing_cert_scope(config),
                  "project:releng:signing:format:mar"]

        upstream_artifacts = [{
            "taskId": {"task-reference": "<repackage>"},
            "taskType": "repackage",
            "paths": [
                "public/build/{}target.complete.mar".format(locale_str),
            ],
            "formats": ["mar"]
        }]
        if 'win' in dep_job.attributes.get('build_platform'):
            upstream_artifacts.append({
                "taskId": {"task-reference": "<repackage>"},
                "taskType": "repackage",
                "paths": [
                    "public/build/{}target.installer.exe".format(locale_str),
                ],
                "formats": ["sha2signcode"]
            })
            scopes.append("project:releng:signing:format:sha2signcode")

            # Stub installer is only generated on win32
            if '32' in dep_job.attributes.get('build_platform'):
                upstream_artifacts.append({
                    "taskId": {"task-reference": "<repackage>"},
                    "taskType": "repackage",
                    "paths": [
                        "public/build/{}target.stub-installer.exe".format(locale_str),
                    ],
                    "formats": ["sha2signcodestub"]
                })
                scopes.append("project:releng:signing:format:sha2signcodestub")

        task = {
            'label': label,
            'description': "Repackage signing {} ".format(
                dep_job.task["metadata"]["description"]),
            'worker-type': "scriptworker-prov-v1/signing-linux-v1",
            'worker': {'implementation': 'scriptworker-signing',
                       'upstream-artifacts': upstream_artifacts,
                       'max-run-time': 3600},
            'scopes': scopes,
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
        }

        funsize_platforms = [
            'macosx64-nightly',
            'win32-nightly',
            'win64-nightly'
        ]
        if dep_job.attributes.get('build_platform') in funsize_platforms and \
                dep_job.attributes.get('nightly'):
            route_template = "project.releng.funsize.level-{level}.{project}"
            task['routes'] = [
                route_template.format(project=config.params['project'],
                                      level=config.params['level'])
            ]

        yield task
