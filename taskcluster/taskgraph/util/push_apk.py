# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Common functions for both push-apk and push-apk-breakpoint.
"""

import re

from taskgraph.util.schema import validate_schema

REQUIRED_ARCHITECTURES = {
    'android-x86-nightly',
    'android-aarch64-nightly',
    'android-api-15-nightly',
}
PLATFORM_REGEX = re.compile(r'signing-android-(\S+)-nightly')


def fill_labels_tranform(_, jobs):
    for job in jobs:
        job['label'] = job['name']

        yield job


def validate_jobs_schema_transform_partial(description_schema, transform_type, config, jobs):
    for job in jobs:
        label = job.get('label', '?no-label?')
        yield validate_schema(
            description_schema, job,
            "In {} ({!r} kind) task for {!r}:".format(transform_type, config.kind, label)
        )


def validate_dependent_tasks_transform(_, jobs):
    for job in jobs:
        check_every_architecture_is_present_in_dependent_tasks(job['dependent-tasks'])
        yield job


def check_every_architecture_is_present_in_dependent_tasks(dependent_tasks):
    dep_platforms = set(t.attributes.get('build_platform') for t in dependent_tasks)
    missed_architectures = REQUIRED_ARCHITECTURES - dep_platforms
    if missed_architectures:
        raise Exception('''One or many required architectures are missing.

Required architectures: {}.
Given dependencies: {}.
'''.format(REQUIRED_ARCHITECTURES, dependent_tasks)
        )


def delete_non_required_fields_transform(_, jobs):
    for job in jobs:
        del job['name']
        del job['dependent-tasks']

        yield job


def generate_dependencies(dependent_tasks):
    # Because we depend on several tasks that have the same kind, we introduce the platform
    dependencies = {}
    for task in dependent_tasks:
        platform_match = PLATFORM_REGEX.match(task.label)
        # platform_match is None when the breakpoint task is given
        task_kind = task.kind if platform_match is None else \
            '{}-{}'.format(task.kind, platform_match.group(1))
        dependencies[task_kind] = task.label
    return dependencies
