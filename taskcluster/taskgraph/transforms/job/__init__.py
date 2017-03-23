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
import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import (
    validate_schema,
    Schema,
)
from taskgraph.transforms.task import task_description_schema
from voluptuous import (
    Any,
    Extra,
    Optional,
    Required,
)

logger = logging.getLogger(__name__)

# Voluptuous uses marker objects as dictionary *keys*, but they are not
# comparable, so we cast all of the keys back to regular strings
task_description_schema = {str(k): v for k, v in task_description_schema.schema.iteritems()}

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
    Optional('dependencies'): task_description_schema['dependencies'],
    Optional('expires-after'): task_description_schema['expires-after'],
    Optional('routes'): task_description_schema['routes'],
    Optional('scopes'): task_description_schema['scopes'],
    Optional('tags'): task_description_schema['tags'],
    Optional('extra'): task_description_schema['extra'],
    Optional('treeherder'): task_description_schema['treeherder'],
    Optional('index'): task_description_schema['index'],
    Optional('run-on-projects'): task_description_schema['run-on-projects'],
    Optional('coalesce-name'): task_description_schema['coalesce-name'],
    Optional('optimizations'): task_description_schema['optimizations'],
    Optional('needs-sccache'): task_description_schema['needs-sccache'],

    # The "when" section contains descriptions of the circumstances
    # under which this task should be included in the task graph.  This
    # will be converted into an element in the `optimizations` list.
    Optional('when'): Any({
        # This task only needs to be run if a file matching one of the given
        # patterns has changed in the push.  The patterns use the mozpack
        # match function (python/mozbuild/mozpack/path.py).
        Optional('files-changed'): [basestring],
    }),

    # A description of how to run this job.
    'run': {
        # The key to a job implementation in a peer module to this one
        'using': basestring,

        # Any remaining content is verified against that job implementation's
        # own schema.
        Extra: object,
    },

    Required('worker-type'): Any(
        task_description_schema['worker-type'],
        {'by-platform': {basestring: task_description_schema['worker-type']}},
    ),
    Required('worker'): Any(
        task_description_schema['worker'],
        {'by-platform': {basestring: task_description_schema['worker']}},
    ),
})

transforms = TransformSequence()


@transforms.add
def validate(config, jobs):
    for job in jobs:
        yield validate_schema(job_description_schema, job,
                              "In job {!r}:".format(job['name']))


@transforms.add
def rewrite_when_to_optimization(config, jobs):
    for job in jobs:
        when = job.pop('when', {})
        files_changed = when.get('files-changed')
        if not files_changed:
            yield job
            continue

        # add some common files
        files_changed.extend([
            '{}/**'.format(config.path),
            'taskcluster/taskgraph/**',
        ])
        if 'in-tree' in job['worker'].get('docker-image', {}):
            files_changed.append('taskcluster/docker/{}/**'.format(
                job['worker']['docker-image']['in-tree']))

        job.setdefault('optimizations', []).append(['files-changed', files_changed])

        assert 'when' not in job
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
        if job['name']:
            del job['name']
        taskdesc = copy.deepcopy(job)

        # fill in some empty defaults to make run implementations easier
        taskdesc.setdefault('attributes', {})
        taskdesc.setdefault('dependencies', {})
        taskdesc.setdefault('routes', [])
        taskdesc.setdefault('scopes', [])
        taskdesc.setdefault('extra', {})

        # give the function for job.run.using on this worker implementation a
        # chance to set up the task description.
        configure_taskdesc_for_run(config, job, taskdesc)
        del taskdesc['run']

        # yield only the task description, discarding the job description
        yield taskdesc

# A registry of all functions decorated with run_job_using
registry = {}


def run_job_using(worker_implementation, run_using, schema=None):
    """Register the decorated function as able to set up a task description for
    jobs with the given worker implementation and `run.using` property.  If
    `schema` is given, the job's run field will be verified to match it.

    The decorated function should have the signature `using_foo(config, job,
    taskdesc) and should modify the task description in-place.  The skeleton of
    the task description is already set up, but without a payload."""
    def wrap(func):
        for_run_using = registry.setdefault(run_using, {})
        if worker_implementation in for_run_using:
            raise Exception("run_job_using({!r}, {!r}) already exists: {!r}".format(
                run_using, worker_implementation, for_run_using[run_using]))
        for_run_using[worker_implementation] = (func, schema)
        return func
    return wrap


def configure_taskdesc_for_run(config, job, taskdesc):
    """
    Run the appropriate function for this job against the given task
    description.

    This will raise an appropriate error if no function exists, or if the job's
    run is not valid according to the schema.
    """
    run_using = job['run']['using']
    if run_using not in registry:
        raise Exception("no functions for run.using {!r}".format(run_using))

    worker_implementation = job['worker']['implementation']
    if worker_implementation not in registry[run_using]:
        raise Exception("no functions for run.using {!r} on {!r}".format(
            run_using, worker_implementation))

    func, schema = registry[run_using][worker_implementation]
    if schema:
        job['run'] = validate_schema(
                schema, job['run'],
                "In job.run using {!r} for job {!r}:".format(
                    job['run']['using'], job['label']))

    func(config, job, taskdesc)


def import_all():
    """Import all modules that are siblings of this one, triggering the decorator
    above in the process."""
    for f in os.listdir(os.path.dirname(__file__)):
        if f.endswith('.py') and f not in ('commmon.py', '__init__.py'):
            __import__('taskgraph.transforms.job.' + f[:-3])
