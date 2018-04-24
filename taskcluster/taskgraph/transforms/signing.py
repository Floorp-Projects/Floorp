# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.schema import validate_schema, Schema
from taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_signing_cert_scope_per_platform,
    get_worker_type_for_scope,
)
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

signing_description_schema = Schema({
    # the dependant task (object) for this signing job, used to inform signing.
    Required('dependent-task'): object,

    # Artifacts from dep task to sign - Sync with taskgraph/transforms/task.py
    # because this is passed directly into the signingscript worker
    Required('upstream-artifacts'): [{
        # taskId of the task with the artifact
        Required('taskId'): taskref_or_string,

        # type of signing task (for CoT)
        Required('taskType'): basestring,

        # Paths to the artifacts to sign
        Required('paths'): [basestring],

        # Signing formats to use on each of the paths
        Required('formats'): [basestring],
    }],

    # depname is used in taskref's to identify the taskID of the unsigned things
    Required('depname'): basestring,

    # unique label to describe this signing task, defaults to {dep.label}-signing
    Optional('label'): basestring,

    # treeherder is allowed here to override any defaults we use for signing.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details, and the
    # below transforms for defaults of various values.
    Optional('treeherder'): task_description_schema['treeherder'],

    # Routes specific to this task, if defined
    Optional('routes'): [basestring],

    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],

    # Optional control for how long a task may run (aka maxRunTime)
    Optional('max-run-time'): int,
    Optional('extra'): {basestring: object},
})


@transforms.add
def set_defaults(config, jobs):
    for job in jobs:
        job.setdefault('depname', 'build')
        yield job


@transforms.add
def validate(config, jobs):
    for job in jobs:
        label = job.get('dependent-task', object).__dict__.get('label', '?no-label?')
        validate_schema(
            signing_description_schema, job,
            "In signing ({!r} kind) task for {!r}:".format(config.kind, label))
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job['dependent-task']
        attributes = dep_job.attributes

        signing_format_scopes = []
        formats = set([])
        for artifacts in job['upstream-artifacts']:
            for f in artifacts['formats']:
                formats.add(f)  # Add each format only once
        for format in formats:
            signing_format_scopes.append(
                add_scope_prefix(config, 'signing:format:{}'.format(format))
            )

        is_nightly = dep_job.attributes.get('nightly', False)
        treeherder = None
        if 'partner' not in config.kind and 'eme-free' not in config.kind:
            treeherder = job.get('treeherder', {})
            treeherder.setdefault('symbol', _generate_treeherder_symbol(is_nightly, config.kind))

            dep_th_platform = dep_job.task.get('extra', {}).get(
                'treeherder', {}).get('machine', {}).get('platform', '')
            build_type = dep_job.attributes.get('build_type')
            build_platform = dep_job.attributes.get('build_platform')
            treeherder.setdefault('platform', _generate_treeherder_platform(
                dep_th_platform, build_platform, build_type
            ))

            treeherder.setdefault('tier', 1 if '-ccov' not in build_platform else 2)
            treeherder.setdefault('kind', 'build')

        label = job['label']
        description = (
            "Initial Signing for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get('locale', 'en-US'),
                build_platform=attributes.get('build_platform'),
                build_type=attributes.get('build_type')
            )
        )

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes['signed'] = True

        if dep_job.attributes.get('chunk_locales'):
            # Used for l10n attribute passthrough
            attributes['chunk_locales'] = dep_job.attributes.get('chunk_locales')

        signing_cert_scope = get_signing_cert_scope_per_platform(
            dep_job.attributes.get('build_platform'), is_nightly, config
        )

        task = {
            'label': label,
            'description': description,
            'worker-type': get_worker_type_for_scope(config, signing_cert_scope),
            'worker': {'implementation': 'scriptworker-signing',
                       'upstream-artifacts': job['upstream-artifacts'],
                       'max-run-time': job.get('max-run-time', 3600)},
            'scopes': [signing_cert_scope] + signing_format_scopes,
            'dependencies': {job['depname']: dep_job.label},
            'attributes': attributes,
            'run-on-projects': dep_job.attributes.get('run_on_projects'),
            'optimization': dep_job.optimization,
            'routes': job.get('routes', []),
            'shipping-product': job.get('shipping-product'),
            'shipping-phase': job.get('shipping-phase'),
        }
        if treeherder:
            task['treeherder'] = treeherder
        if job.get('extra'):
            task['extra'] = job['extra']

        yield task


def _generate_treeherder_platform(dep_th_platform, build_platform, build_type):
    if '-pgo' in build_platform:
        actual_build_type = 'pgo'
    elif '-ccov' in build_platform:
        actual_build_type = 'ccov'
    else:
        actual_build_type = build_type
    return '{}/{}'.format(dep_th_platform, actual_build_type)


def _generate_treeherder_symbol(is_nightly, kind):
    if is_nightly:
        return 'Ns'
    else:
        return 'Bs'
