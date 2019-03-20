# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Convert a job description into a task description.

Jobs descriptions are similar to task descriptions, but they specify how to run
the job at a higher level, using a "run" field that can be interpreted by
run-using handlers in `taskcluster/taskgraph/transforms/job`.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy
import logging
import json
import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import (
    validate_schema,
    Schema,
)
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.workertypes import worker_type_implementation
from taskgraph.transforms.task import task_description_schema
from voluptuous import (
    Extra,
    Optional,
    Required,
    Exclusive,
)

logger = logging.getLogger(__name__)

# Schema for a build description
job_description_schema = Schema({
    # The name of the job and the job's label.  At least one must be specified,
    # and the label will be generated from the name if necessary, by prepending
    # the kind.
    Optional('name'): basestring,
    Optional('label'): basestring,

    # the following fields are passed directly through to the task description,
    # possibly modified by the run implementation.  See
    # taskcluster/taskgraph/transforms/task.py for the schema details.
    Required('description'): task_description_schema['description'],
    Optional('attributes'): task_description_schema['attributes'],
    Optional('job-from'): task_description_schema['job-from'],
    Optional('dependencies'): task_description_schema['dependencies'],
    Optional('soft-dependencies'): task_description_schema['soft-dependencies'],
    Optional('requires'): task_description_schema['requires'],
    Optional('expires-after'): task_description_schema['expires-after'],
    Optional('routes'): task_description_schema['routes'],
    Optional('scopes'): task_description_schema['scopes'],
    Optional('tags'): task_description_schema['tags'],
    Optional('extra'): task_description_schema['extra'],
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('index'): task_description_schema['index'],
    Optional('run-on-projects'): task_description_schema['run-on-projects'],
    Optional('shipping-phase'): task_description_schema['shipping-phase'],
    Optional('shipping-product'): task_description_schema['shipping-product'],
    Optional('coalesce'): task_description_schema['coalesce'],
    Optional('always-target'): task_description_schema['always-target'],
    Exclusive('optimization', 'optimization'): task_description_schema['optimization'],
    Optional('needs-sccache'): task_description_schema['needs-sccache'],
    Optional('release-artifacts'): task_description_schema['release-artifacts'],

    # The "when" section contains descriptions of the circumstances under which
    # this task should be included in the task graph.  This will be converted
    # into an optimization, so it cannot be specified in a job description that
    # also gives 'optimization'.
    Exclusive('when', 'optimization'): {
        # This task only needs to be run if a file matching one of the given
        # patterns has changed in the push.  The patterns use the mozpack
        # match function (python/mozbuild/mozpack/path.py).
        Optional('files-changed'): [basestring],
    },

    # A list of artifacts to install from 'fetch' tasks.
    Optional('fetches'): {
        basestring: [basestring, {
            Required('artifact'): basestring,
            Optional('dest'): basestring,
            Optional('extract'): bool,
        }],
    },

    # A description of how to run this job.
    'run': {
        # The key to a job implementation in a peer module to this one
        'using': basestring,

        # Base work directory used to set up the task.
        Optional('workdir'): basestring,

        # Any remaining content is verified against that job implementation's
        # own schema.
        Extra: object,
    },

    Required('worker-type'): task_description_schema['worker-type'],

    # This object will be passed through to the task description, with additions
    # provided by the job's run-using function
    Optional('worker'): dict,
})

transforms = TransformSequence()
transforms.add_validate(job_description_schema)


@transforms.add
def rewrite_when_to_optimization(config, jobs):
    for job in jobs:
        when = job.pop('when', {})
        if not when:
            yield job
            continue

        files_changed = when.get('files-changed')

        # implicitly add task config directory.
        files_changed.append('{}/**'.format(config.path))

        # "only when files changed" implies "skip if files have not changed"
        job['optimization'] = {'skip-unless-changed': files_changed}

        assert 'when' not in job
        yield job


@transforms.add
def set_implementation(config, jobs):
    for job in jobs:
        impl, os = worker_type_implementation(config.graph_config, job['worker-type'])
        if os:
            job.setdefault('tags', {})['os'] = os
        if impl:
            job.setdefault('tags', {})['worker-implementation'] = impl
        worker = job.setdefault('worker', {})
        assert 'implementation' not in worker
        worker['implementation'] = impl
        if os:
            worker['os'] = os
        yield job


def get_attribute(dict, key, attributes, attribute_name):
    '''Get `attribute_name` from the given `attributes` dict, and if there
    is a corresponding value, set `key` in `dict` to that value.'''
    value = attributes.get(attribute_name)
    if value:
        dict[key] = value


@transforms.add
def use_fetches(config, jobs):
    artifact_names = {}

    for task in config.kind_dependencies_tasks:
        if task.kind in ('fetch', 'toolchain'):
            get_attribute(
                artifact_names, task.label, task.attributes,
                '{kind}-artifact'.format(kind=task.kind),
            )

    for job in jobs:
        fetches = job.pop('fetches', None)
        if not fetches:
            yield job
            continue

        job_fetches = []
        name = job.get('name', job.get('label'))
        dependencies = job.setdefault('dependencies', {})
        prefix = get_artifact_prefix(job)
        for kind, artifacts in fetches.items():
            if kind in ('fetch', 'toolchain'):
                for fetch_name in artifacts:
                    label = '{kind}-{name}'.format(kind=kind, name=fetch_name)
                    if label not in artifact_names:
                        raise Exception('Missing fetch job for {kind}-{name}: {fetch}'.format(
                            kind=config.kind, name=name, fetch=fetch_name))

                    path = artifact_names[label]
                    if not path.startswith('public/'):
                        raise Exception(
                            'Non-public artifacts not supported for {kind}-{name}: '
                            '{fetch}'.format(kind=config.kind, name=name, fetch=fetch_name))

                    dependencies[label] = label
                    job_fetches.append({
                        'artifact': path,
                        'task': '<{label}>'.format(label=label),
                        'extract': True,
                    })
            else:
                if kind not in dependencies:
                    raise Exception("{name} can't fetch {kind} artifacts because "
                                    "it has no {kind} dependencies!".format(name=name, kind=kind))

                for artifact in artifacts:
                    if isinstance(artifact, basestring):
                        path = artifact
                        dest = None
                        extract = True
                    else:
                        path = artifact['artifact']
                        dest = artifact.get('dest')
                        extract = artifact.get('extract', True)

                    fetch = {
                        'artifact': '{prefix}/{path}'.format(prefix=prefix, path=path),
                        'task': '<{dep}>'.format(dep=kind),
                        'extract': extract,
                    }
                    if dest is not None:
                        fetch['dest'] = dest
                    job_fetches.append(fetch)

        env = job.setdefault('worker', {}).setdefault('env', {})
        env['MOZ_FETCHES'] = {'task-reference': json.dumps(job_fetches, sort_keys=True)}

        if job['worker']['os'] in ('windows', 'macosx'):
            env.setdefault('MOZ_FETCHES_DIR', 'fetches')
        else:
            workdir = job['run'].get('workdir', '/builds/worker')
            env.setdefault('MOZ_FETCHES_DIR', '{}/fetches'.format(workdir))

        yield job


@transforms.add
def make_task_description(config, jobs):
    """Given a build description, create a task description"""
    # import plugin modules first, before iterating over jobs
    import_all()
    for job in jobs:
        if 'label' not in job:
            if 'name' not in job:
                raise Exception("job has neither a name nor a label")
            job['label'] = '{}-{}'.format(config.kind, job['name'])
        if job.get('name'):
            del job['name']

        # always-optimized tasks never execute, so have no workdir
        if job['run']['using'] != 'always-optimized':
            job['run'].setdefault('workdir', '/builds/worker')

        taskdesc = copy.deepcopy(job)

        # fill in some empty defaults to make run implementations easier
        taskdesc.setdefault('attributes', {})
        taskdesc.setdefault('dependencies', {})
        taskdesc.setdefault('soft-dependencies', [])
        taskdesc.setdefault('routes', [])
        taskdesc.setdefault('scopes', [])
        taskdesc.setdefault('extra', {})

        # give the function for job.run.using on this worker implementation a
        # chance to set up the task description.
        configure_taskdesc_for_run(config, job, taskdesc, job['worker']['implementation'])
        del taskdesc['run']

        # yield only the task description, discarding the job description
        yield taskdesc


# A registry of all functions decorated with run_job_using
registry = {}


def run_job_using(worker_implementation, run_using, schema=None, defaults={}):
    """Register the decorated function as able to set up a task description for
    jobs with the given worker implementation and `run.using` property.  If
    `schema` is given, the job's run field will be verified to match it.

    The decorated function should have the signature `using_foo(config, job, taskdesc)`
    and should modify the task description in-place.  The skeleton of
    the task description is already set up, but without a payload."""
    def wrap(func):
        for_run_using = registry.setdefault(run_using, {})
        if worker_implementation in for_run_using:
            raise Exception("run_job_using({!r}, {!r}) already exists: {!r}".format(
                run_using, worker_implementation, for_run_using[run_using]))
        for_run_using[worker_implementation] = (func, schema, defaults)
        return func
    return wrap


@run_job_using('always-optimized', 'always-optimized',
               Schema({'using': 'always-optimized'}))
def always_optimized(config, job, taskdesc):
    pass


def configure_taskdesc_for_run(config, job, taskdesc, worker_implementation):
    """
    Run the appropriate function for this job against the given task
    description.

    This will raise an appropriate error if no function exists, or if the job's
    run is not valid according to the schema.
    """
    run_using = job['run']['using']
    if run_using not in registry:
        raise Exception("no functions for run.using {!r}".format(run_using))

    if worker_implementation not in registry[run_using]:
        raise Exception("no functions for run.using {!r} on {!r}".format(
            run_using, worker_implementation))

    func, schema, defaults = registry[run_using][worker_implementation]
    for k, v in defaults.items():
        job['run'].setdefault(k, v)

    if schema:
        validate_schema(
                schema, job['run'],
                "In job.run using {!r}/{!r} for job {!r}:".format(
                    job['run']['using'], worker_implementation, job['label']))
    func(config, job, taskdesc)


def import_all():
    """Import all modules that are siblings of this one, triggering the decorator
    above in the process."""
    for f in os.listdir(os.path.dirname(__file__)):
        if f.endswith('.py') and f not in ('commmon.py', '__init__.py'):
            __import__('taskgraph.transforms.job.' + f[:-3])
