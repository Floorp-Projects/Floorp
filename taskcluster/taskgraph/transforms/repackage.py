# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

_TC_ARTIFACT_LOCATION = \
        'https://queue.taskcluster.net/v1/task/{task_id}/artifacts/public/build/{postfix}'

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
        signing_dependencies = dep_job.dependencies
        # This is so we get the build task in our dependencies to
        # have better beetmover support.
        dependencies.update(signing_dependencies)

        treeherder = job.get('treeherder', {})
        treeherder.setdefault('symbol', 'tc(Nr)')
        dep_th_platform = dep_job.task.get('extra', {}).get(
            'treeherder', {}).get('machine', {}).get('platform', '')
        treeherder.setdefault('platform', "{}/opt".format(dep_th_platform))
        treeherder.setdefault('tier', 1)
        treeherder.setdefault('kind', 'build')
        build_task = None
        signing_task = None
        for dependency in dependencies.keys():
            if 'signing' in dependency:
                signing_task = dependency
            else:
                build_task = dependency
        if job.get('locale'):
            # XXXCallek: todo: rewrite dependency finding
            # Use string splice to strip out 'nightly-l10n-' .. '-<chunk>/opt'
            # We need this additional dependency to support finding the mar binary
            # Which is needed in order to generate a new complete.mar
            dependencies['build'] = "build-{}/opt".format(
                dependencies[build_task][13:dependencies[build_task].rfind('-')])
            build_task = 'build'
        signing_task_ref = "<{}>".format(signing_task)
        build_task_ref = "<{}>".format(build_task)

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['repackage_type'] = 'repackage'

        locale = None
        if job.get('locale'):
            locale = job['locale']
            attributes['locale'] = locale

        level = config.params['level']

        build_platform = attributes['build_platform']
        run = {
            'using': 'mozharness',
            'script': 'mozharness/scripts/repackage.py',
            'config': _generate_task_mozharness_config(build_platform),
            'job-script': 'taskcluster/scripts/builder/repackage.sh',
            'actions': ['download_input', 'setup', 'repackage'],
            'extra-workspace-cache-key': 'repackage',
        }

        worker = {
            'env': _generate_task_env(build_platform, build_task_ref,
                                      signing_task_ref, locale=locale),
            'artifacts': _generate_task_output_files(build_platform, locale=locale),
            'chain-of-trust': True,
            'max-run-time': 3600,
        }

        if build_platform.startswith('macosx'):
            worker_type = 'aws-provisioner-v1/gecko-%s-b-macosx64' % level

            run['tooltool-downloads'] = 'internal'
            worker['docker-image'] = {"in-tree": "desktop-build"},

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


def _generate_task_mozharness_config(build_platform):
    if build_platform.startswith('macosx'):
        return ['repackage/osx_signed.py']
    else:
        raise NotImplemented('Unsupported build_platform: "{}"'.format(build_platform))


def _generate_task_env(build_platform, build_task_ref, signing_task_ref, locale=None):
    mar_prefix = _generate_taskcluster_prefix(build_task_ref, postfix='host/bin/', locale=None)
    signed_prefix = _generate_taskcluster_prefix(signing_task_ref, locale=locale)

    if build_platform.startswith('macosx'):
        return {
            'SIGNED_INPUT': {'task-reference': '{}target.tar.gz'.format(signed_prefix)},
            'UNSIGNED_MAR': {'task-reference': '{}mar'.format(mar_prefix)},
        }
    else:
        raise NotImplemented('Unsupported build_platform: "{}"'.format(build_platform))


def _generate_taskcluster_prefix(task_id, postfix='', locale=None):
    if locale:
        postfix = '{}/{}'.format(locale, postfix)

    return _TC_ARTIFACT_LOCATION.format(task_id=task_id, postfix=postfix)


def _generate_task_output_files(build_platform, locale=None):
    locale_output_path = '{}/'.format(locale) if locale else ''
    if build_platform.startswith('macosx'):
        return [{
            'type': 'file',
            'path': '/home/worker/workspace/build/artifacts/target.dmg',
            'name': 'public/build/{}target.dmg'.format(locale_output_path),
        }, {
            'type': 'file',
            'path': '/home/worker/workspace/build/artifacts/target.complete.mar',
            'name': 'public/build/{}target.complete.mar'.format(locale_output_path),
        }]
    else:
        raise NotImplemented('Unsupported build_platform: "{}"'.format(build_platform))
