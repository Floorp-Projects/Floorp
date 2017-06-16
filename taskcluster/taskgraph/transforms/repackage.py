# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

transforms = TransformSequence()

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

# shortcut for a string where task references are allowed
taskref_or_string = Any(
    basestring,
    {Required('task-reference'): basestring})

packaging_description_schema = Schema({
    # the dependant task (object) for this  job, used to inform repackaging.
    Required('dependent-task'): object,

    # depname is used in taskref's to identify the taskID of the signed things
    Required('depname', default='build'): basestring,

    # unique label to describe this repackaging task
    Optional('label'): basestring,

    # treeherder is allowed here to override any defaults we use for repackaging.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],

    # If a l10n task, the corresponding locale
    Optional('locale'): basestring,

    # Routes specific to this task, if defined
    Optional('routes'): [basestring],

    # passed through directly to the job description
    Optional('extra'): task_description_schema['extra'],

})


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        yield validate_schema(
            packaging_description_schema, job,
            "In packaging ({!r} kind) task for {!r}:".format(config.kind, label))


@transforms.add
def make_repackage_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']

        label = job.get('label',
                        dep_job.label.replace("signing-", "repackage-"))
        job['label'] = label

        yield job


@transforms.add
def make_job_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        dependencies = {dep_job.attributes.get('kind'): dep_job.label}
        if len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't repackage a signing task with multiple dependencies")

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'tc(Nr)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform', "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')
        signing_task = None
        for dependency in dependencies.keys():
            if 'signing' in dependency:
                signing_task = dependency
        signing_task_ref = "<{}>".format(signing_task)

        attributes = {
            'nightly': dep_job.attributes.get('nightly', False),
            'build_platform': dep_job.attributes.get('build_platform'),
            'build_type': dep_job.attributes.get('build_type'),
        }
        if job.get('locale'):
            attributes['locale'] = job['locale']

        level = config.params['level']

        task_env = {}
        locale_output_path = ""
        if attributes['build_platform'].startswith('macosx'):
            if job.get('locale'):
                input_string = 'https://queue.taskcluster.net/v1/task/' + \
                    '{}/artifacts/public/build/{}/target.tar.gz'
                input_string = input_string.format(signing_task_ref, job['locale'])
                locale_output_path = "{}/".format(job['locale'])
            else:
                input_string = 'https://queue.taskcluster.net/v1/task/' + \
                    '{}/artifacts/public/build/target.tar.gz'.format(signing_task_ref)
            task_env.update(
                SIGNED_INPUT={'task-reference': input_string},
            )
            mozharness_config = ['repackage/osx_signed.py']
            output_files = [{
                'type': 'file',
                'path': '/home/worker/workspace/build/artifacts/target.dmg',
                'name': 'public/build/{}target.dmg'.format(locale_output_path),
            }]
        else:
            raise Exception("Unexpected build platform for repackage")

        run = {
            'using': 'mozharness',
            'script': 'mozharness/scripts/repackage.py',
            'config': mozharness_config,
            'job-script': 'taskcluster/scripts/builder/repackage.sh',
            'actions': ['download_input', 'setup', 'repackage'],
            'extra-workspace-cache-key': 'repackage',
        }

        if attributes["build_platform"].startswith('macosx'):
            worker = {
                'docker-image': {"in-tree": "desktop-build"},
                'artifacts': output_files,
                'env': task_env,
                'chain-of-trust': True,
                'max-run-time': 3600
            }
            run["tooltool-downloads"] = 'internal'
            worker_type = 'aws-provisioner-v1/gecko-%s-b-macosx64' % level
            cot = job.setdefault('extra', {}).setdefault('chainOfTrust', {})
            cot.setdefault('inputs', {})['docker-image'] = {"task-reference": "<docker-image>"}

        task = {
            'label': job['label'],
            'description': "{} Repackage".format(
                dep_job.task["metadata"]["description"]),
            'worker-type': worker_type,
            'dependencies': dependencies,
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'treeherder': treeherder,
            'routes': job.get('routes', []),
            'extra': job.get('extra', {}),
            'worker': worker,
            'run': run,
        }
        yield task
