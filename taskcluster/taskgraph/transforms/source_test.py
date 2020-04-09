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
import six
from six import text_type

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.job import job_description_schema
from taskgraph.util.attributes import keymatch
from taskgraph.util.schema import (
    resolve_keyed_by,
    optionally_keyed_by,
)
from taskgraph.util.treeherder import join_symbol, split_symbol

from voluptuous import (
    Any,
    Extra,
    Optional,
    Required,
    Schema,
)

source_test_description_schema = Schema({
    # most fields are passed directly through as job fields, and are not
    # repeated here
    Extra: object,

    # The platform on which this task runs.  This will be used to set up attributes
    # (for try selection) and treeherder metadata (for display).  If given as a list,
    # the job will be "split" into multiple tasks, one with each platform.
    Required('platform'): Any(text_type, [text_type]),

    # Build labels required for the task. If this key is provided it must
    # contain a build label for the task platform.
    # The task will then depend on a build task, and the installer url will be
    # saved to the GECKO_INSTALLER_URL environment variable.
    Optional('require-build'): {
        text_type: text_type,
    },

    # These fields can be keyed by "platform", and are otherwise identical to
    # job descriptions.
    Required('worker-type'): optionally_keyed_by(
        'platform', job_description_schema['worker-type']),

    Required('worker'): optionally_keyed_by(
        'platform', job_description_schema['worker']),

    Optional('python-version'): [int],
    # If true, the DECISION_TASK_ID env will be populated.
    Optional('require-decision-task-id'): bool,

    # A list of artifacts to install from 'fetch' tasks.
    Optional('fetches'): {
        text_type: optionally_keyed_by(
            'platform', job_description_schema['fetches'][text_type]),
    },

})

transforms = TransformSequence()

transforms.add_validate(source_test_description_schema)


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
        if isinstance(job['platform'], text_type):
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


@transforms.add
def split_jsshell(config, jobs):
    all_shells = {
        'sm': "Spidermonkey",
        'v8': "Google V8"
    }

    for job in jobs:
        if not job['name'].startswith('jsshell'):
            yield job
            continue

        test = job.pop('test')
        for shell in job.get('shell', all_shells.keys()):
            assert shell in all_shells

            new_job = copy.deepcopy(job)
            new_job['name'] = '{}-{}'.format(new_job['name'], shell)
            new_job['description'] = '{} on {}'.format(new_job['description'], all_shells[shell])
            new_job['shell'] = shell

            group = 'js-bench-{}'.format(shell)
            symbol = split_symbol(new_job['treeherder']['symbol'])[1]
            new_job['treeherder']['symbol'] = join_symbol(group, symbol)

            run = new_job['run']
            run['mach'] = run['mach'].format(shell=shell, SHELL=shell.upper(), test=test)
            yield new_job


def add_build_dependency(config, job):
    """
    Add build dependency to the job and installer_url to env.
    """
    key = job['platform']
    build_labels = job.pop('require-build', {})
    matches = keymatch(build_labels, key)
    if not matches:
        raise Exception("No build platform found. "
                        "Define 'require-build' for {} in the task config.".format(key))

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
        'fetches.toolchain',
        'worker-type',
        'worker',
    ]

    for job in jobs:
        platform = job['platform']

        for field in fields:
            resolve_keyed_by(job, field, item_name=job['name'])

        if 'treeherder' in job:
            job['treeherder']['platform'] = platform

        if 'require-build' in job:
            add_build_dependency(config, job)

        del job['platform']
        yield job


@transforms.add
def handle_shell(config, jobs):
    """
    Handle the 'shell' property.
    """
    fields = [
        'run-on-projects',
        'worker.env',
    ]

    for job in jobs:
        if not job.get('shell'):
            yield job
            continue

        for field in fields:
            resolve_keyed_by(job, field, item_name=job['name'])

        del job['shell']
        yield job


@transforms.add
def add_decision_task_id_to_env(config, jobs):
    """
    Creates the `DECISION_TASK_ID` environment variable in tasks that set the
    `require-decision-task-id` config.
    """
    for job in jobs:
        if not job.pop('require-decision-task-id', False):
            yield job
            continue

        env = job['worker'].setdefault('env', {})
        env['DECISION_TASK_ID'] = six.ensure_text(os.environ.get('TASK_ID', ''))
        yield job


@transforms.add
def set_code_review_env(config, jobs):
    """
    Add a CODE_REVIEW environment variable when running in code-review bot mode
    """
    is_code_review = config.params['target_tasks_method'] == 'codereview'

    for job in jobs:
        attrs = job.get('attributes', {})
        if is_code_review and attrs.get('code-review') is True:
            env = job['worker'].setdefault('env', {})
            env['CODE_REVIEW'] = '1'

        yield job
