# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Source-test jobs can run on multiple platforms.  These transforms allow jobs
with either `platform` or a list of `platforms`, and set the appropriate
treeherder configuration and attributes for that platform.
"""

from __future__ import absolute_import, print_function, unicode_literals

import copy
import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.job import job_description_schema
from taskgraph.util.attributes import keymatch
from taskgraph.util.schema import (
    validate_schema,
    resolve_keyed_by,
)
from taskgraph.util.treeherder import join_symbol, split_symbol

from voluptuous import (
    Any,
    Extra,
    Optional,
    Required,
    Schema,
)

ARTIFACT_URL = 'https://queue.taskcluster.net/v1/task/{}/artifacts/{}'

job_description_schema = {str(k): v for k, v in job_description_schema.schema.iteritems()}

source_test_description_schema = Schema({
    # most fields are passed directly through as job fields, and are not
    # repeated here
    Extra: object,

    # The platform on which this task runs.  This will be used to set up attributes
    # (for try selection) and treeherder metadata (for display).  If given as a list,
    # the job will be "split" into multiple tasks, one with each platform.
    Required('platform'): Any(basestring, [basestring]),

    # Whether the job requires a build artifact or not. If True, the task will
    # depend on a build task and the installer url will be saved to the
    # GECKO_INSTALLER_URL environment variable. Build labels are determined by the
    # `dependent-build-platforms` config in kind.yml.
    Required('require-build'): bool,

    # These fields can be keyed by "platform", and are otherwise identical to
    # job descriptions.
    Required('worker-type'): Any(
        job_description_schema['worker-type'],
        {'by-platform': {basestring: job_description_schema['worker-type']}},
    ),
    Required('worker'): Any(
        job_description_schema['worker'],
        {'by-platform': {basestring: job_description_schema['worker']}},
    ),
    Optional('python-version'): [int],
})

transforms = TransformSequence()


@transforms.add
def set_defaults(config, jobs):
    for job in jobs:
        job.setdefault('require-build', False)
        yield job


@transforms.add
def validate(config, jobs):
    for job in jobs:
        validate_schema(source_test_description_schema, job,
                        "In job {!r}:".format(job['name']))
        yield job


@transforms.add
def set_job_name(config, jobs):
    for job in jobs:
        if 'job-from' in job and job['job-from'] != 'kind.yml':
            from_name = os.path.splitext(job['job-from'])[0]
            job['name'] = '{}-{}'.format(from_name, job['name'])
        yield job


@transforms.add
def expand_platforms(config, jobs):
    for job in jobs:
        if isinstance(job['platform'], basestring):
            yield job
            continue

        for platform in job['platform']:
            pjob = copy.deepcopy(job)
            pjob['platform'] = platform

            if 'name' in pjob:
                pjob['name'] = '{}-{}'.format(pjob['name'], platform)
            else:
                pjob['label'] = '{}-{}'.format(pjob['label'], platform)
            yield pjob


@transforms.add
def split_python(config, jobs):
    for job in jobs:
        key = 'python-version'
        versions = job.pop(key, [])
        if not versions:
            yield job
            continue
        for version in versions:
            group = 'py{0}'.format(version)
            pyjob = copy.deepcopy(job)
            if 'name' in pyjob:
                pyjob['name'] += '-{0}'.format(group)
            else:
                pyjob['label'] += '-{0}'.format(group)
            symbol = split_symbol(pyjob['treeherder']['symbol'])[1]
            pyjob['treeherder']['symbol'] = join_symbol(group, symbol)
            pyjob['run'][key] = version
            yield pyjob


def add_build_dependency(config, job):
    """
    Add build dependency to the job and installer_url to env.
    """
    key = job['platform']
    build_labels = config.config.get('dependent-build-platforms', {})
    matches = keymatch(build_labels, key)
    if not matches:
        raise Exception("No build platform found for '{}'. "
                        "Define 'dependent-build-platforms' in the kind config.".format(key))

    if len(matches) > 1:
        raise Exception("More than one build platform found for '{}'.".format(key))

    label = matches[0]
    deps = job.setdefault('dependencies', {})
    deps.update({'build': label})


@transforms.add
def handle_platform(config, jobs):
    """
    Handle the 'platform' property, setting up treeherder context as well as
    try-related attributes.
    """
    fields = [
        'worker-type',
        'worker',
    ]

    for job in jobs:
        platform = job['platform']

        for field in fields:
            resolve_keyed_by(job, field, item_name=job['name'])

        if 'treeherder' in job:
            job['treeherder']['platform'] = platform

        if job.pop('require-build'):
            add_build_dependency(config, job)

        del job['platform']
        yield job
