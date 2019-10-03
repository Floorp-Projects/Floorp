# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from six import text_type

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def handle_default_gradle_commands(config, tasks):
    for task in tasks:
        if task.pop('include-default-gradle-commands'):
            task['run']['gradlew'] = [
                ':{component}:assemble',
                ':{component}:assembleAndroidTest',
                ':{component}:test',
                ':{component}:{lint_task}',
            ]

        yield task


@transforms.add
def handle_coverage(config, tasks):
    for task in tasks:
        if task.pop('include-coverage'):
            task['run']['gradlew'].insert(0, '-Pcoverage')

        yield task


@transforms.add
def format_component_name_and_lint(config, tasks):
    for task in tasks:
        lint_task = 'lintRelease' if task.pop("lint-release") else 'lint'

        for field in ('description', 'run.gradlew', 'treeherder.symbol'):
            _deep_format(task, field, component=task["name"], lint_task=lint_task)

        yield task


def _deep_format(object, field, **format_kwargs):
    keys = field.split('.')
    last_key = keys[-1]
    for key in keys[:-1]:
        object = object[key]

    one_before_last_object = object
    object = object[last_key]

    if isinstance(object, text_type):
        one_before_last_object[last_key] = object.format(**format_kwargs)
    elif isinstance(object, list):
        one_before_last_object[last_key] = [item.format(**format_kwargs) for item in object]
    else:
        raise ValueError('Unsupported type for object: {}'.format(object))
