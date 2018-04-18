# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import (
    validate_schema,
    optionally_keyed_by,
    resolve_keyed_by,
    Schema,
)
from taskgraph.util.taskcluster import get_taskcluster_artifact_prefix, get_artifact_prefix
from taskgraph.util.partners import check_if_partners_enabled
from taskgraph.transforms.task import task_description_schema
from voluptuous import Any, Required, Optional

transforms = TransformSequence()

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}


def _by_platform(arg):
    return optionally_keyed_by('build-platform', arg)


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

    # Routes specific to this task, if defined
    Optional('routes'): [basestring],

    # passed through directly to the job description
    Optional('extra'): task_description_schema['extra'],

    # Shipping product and phase
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],

    # All l10n jobs use mozharness
    Required('mozharness'): {
        # Config files passed to the mozharness script
        Required('config'): _by_platform([basestring]),

        # Additional paths to look for mozharness configs in. These should be
        # relative to the base of the source checkout
        Optional('config-paths'): [basestring],

        # if true, perform a checkout of a comm-central based branch inside the
        # gecko checkout
        Required('comm-checkout', default=False): bool,
    }
})

transforms.add(check_if_partners_enabled)


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            packaging_description_schema, job,
            "In packaging ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def copy_in_useful_magic(config, jobs):
    """Copy attributes from upstream task to be used for keyed configuration."""
    for job in jobs:
        dep = job['dependent-task']
        job['build-platform'] = dep.attributes.get("build_platform")
        yield job


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "mozharness.config",
    ]
    for job in jobs:
        job = copy.deepcopy(job)  # don't overwrite dict values here
        for field in fields:
            resolve_keyed_by(item=job, field=field, item_name="?")
        yield job


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
        attributes = copy_attributes_from_dependent_job(dep_job)
        build_platform = attributes['build_platform']

        if job['build-platform'].startswith('win'):
            if dep_job.kind.endswith('signing'):
                continue
        if job['build-platform'].startswith('macosx'):
            if dep_job.kind.endswith('repack'):
                continue
        dependencies = {dep_job.attributes.get('kind'): dep_job.label}
        dependencies.update(dep_job.dependencies)

        signing_task = None
        for dependency in dependencies.keys():
            if build_platform.startswith('macosx') and dependency.endswith('signing'):
                signing_task = dependency
            elif build_platform.startswith('win') and dependency.endswith('repack'):
                signing_task = dependency
        signing_task_ref = "<{}>".format(signing_task)

        attributes['repackage_type'] = 'repackage'

        level = config.params['level']
        repack_id = job['extra']['repack_id']

        run = job.get('mozharness', {})
        run.update({
            'using': 'mozharness',
            'script': 'mozharness/scripts/repackage.py',
            'job-script': 'taskcluster/scripts/builder/repackage.sh',
            'actions': ['download_input', 'setup', 'repackage'],
            'extra-workspace-cache-key': 'repackage',
        })

        worker = {
            'env': _generate_task_env(build_platform, signing_task, signing_task_ref,
                                      partner=repack_id),
            'artifacts': _generate_task_output_files(dep_job, build_platform, partner=repack_id),
            'chain-of-trust': True,
            'max-run-time': 7200 if build_platform.startswith('win') else 3600,
            'taskcluster-proxy': True if get_artifact_prefix(dep_job) else False,
        }

        worker['env'].update(REPACK_ID=repack_id)

        if build_platform.startswith('win'):
            worker_type = 'aws-provisioner-v1/gecko-%s-b-win2012' % level
            run['use-magic-mh-args'] = False
        else:
            if build_platform.startswith('macosx'):
                worker_type = 'aws-provisioner-v1/gecko-%s-b-macosx64' % level
            else:
                raise NotImplementedError(
                    'Unsupported build_platform: "{}"'.format(build_platform)
                )

            run['tooltool-downloads'] = 'internal'
            worker['docker-image'] = {"in-tree": "debian7-amd64-build"}

        description = (
            "Repackaging for repack_id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=job['extra']['repack_id'],
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        task = {
            'label': job['label'],
            'description': description,
            'worker-type': worker_type,
            'dependencies': dependencies,
            'attributes': attributes,
            'scopes': ['queue:get-artifact:releng/partner/*'],
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'routes': job.get('routes', []),
            'extra': job.get('extra', {}),
            'worker': worker,
            'run': run,
        }

        if build_platform.startswith('macosx'):
            task['toolchains'] = [
                'linux64-libdmg',
                'linux64-hfsplus',
            ]
        yield task


def _generate_task_env(build_platform, signing_task, signing_task_ref, partner):
    # Force private artifacts here, until we can populate our dependency map
    # with actual task definitions rather than labels.
    # (get_taskcluster_artifact_prefix requires the task definition to find
    # the artifact_prefix attribute).
    signed_prefix = get_taskcluster_artifact_prefix(
        signing_task, signing_task_ref, locale=partner, force_private=True
    )
    signed_prefix = signed_prefix.replace('public/build', 'releng/partner')

    if build_platform.startswith('macosx'):
        return {
            'SIGNED_INPUT': {'task-reference': '{}target.tar.gz'.format(signed_prefix)},
        }
    elif build_platform.startswith('win'):
        task_env = {
            'SIGNED_ZIP': {'task-reference': '{}target.zip'.format(signed_prefix)},
            'SIGNED_SETUP': {'task-reference': '{}setup.exe'.format(signed_prefix)},
        }

        return task_env

    raise NotImplementedError('Unsupported build_platform: "{}"'.format(build_platform))


def _generate_task_output_files(task, build_platform, partner):
    """We carefully generate an explicit list here, but there's an artifacts directory
    too, courtesy of generic_worker_add_artifacts() (windows) or docker_worker_add_artifacts().
    Any errors here are likely masked by that.
    """
    partner_output_path = '{}/'.format(partner)
    artifact_prefix = get_artifact_prefix(task)

    if build_platform.startswith('macosx'):
        output_files = [{
            'type': 'file',
            'path': '/builds/worker/workspace/build/artifacts/{}target.dmg'
                    .format(partner_output_path),
            'name': '{}/{}target.dmg'.format(artifact_prefix, partner_output_path),
        }]

    elif build_platform.startswith('win'):
        output_files = [{
            'type': 'file',
            'path': '{}/{}target.installer.exe'.format(artifact_prefix, partner_output_path),
            'name': '{}/{}target.installer.exe'.format(artifact_prefix, partner_output_path),
        }]

    if output_files:
        return output_files

    raise NotImplementedError('Unsupported build_platform: "{}"'.format(build_platform))
