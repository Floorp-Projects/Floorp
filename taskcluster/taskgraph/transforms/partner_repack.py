# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the partner repack task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config
from taskgraph.util.partners import check_if_partners_enabled


transforms = TransformSequence()


@transforms.add
def resolve_properties(config, tasks):
    for task in tasks:
        for property in ("REPACK_MANIFESTS_URL", ):
            property = "worker.env.{}".format(property)
            resolve_keyed_by(task, property, property, **config.params)

        if task['worker']['env']['REPACK_MANIFESTS_URL'].startswith('git@'):
            task.setdefault('scopes', []).append(
                'secrets:get:project/releng/gecko/build/level-{level}/partner-github-ssh'.format(
                    **config.params
                )
            )

        yield task


@transforms.add
def make_label(config, tasks):
    for task in tasks:
        task['label'] = "{}-{}".format(config.kind, task['name'])
        yield task


@transforms.add
def add_command_arguments(config, tasks):
    release_config = get_release_config(config)
    for task in tasks:
        # add the MOZHARNESS_OPTIONS, eg version=61.0, build-number=1, platform=win64
        task['run']['options'] = [
            'version={}'.format(release_config['version']),
            'build-number={}'.format(release_config['build_number']),
            'platform={}'.format(task['attributes']['build_platform'].split('-')[0]),
        ]

        # The upstream taskIds are stored a special environment variable, because we want to use
        # task-reference's to resolve dependencies, but the string handling of MOZHARNESS_OPTIONS
        # blocks that. It's space-separated string of ids in the end.
        task['worker']['env']['UPSTREAM_TASKIDS'] = {
            'task-reference': ' '.join(['<{}>'.format(dep) for dep in task['dependencies']])
        }

        yield task


# This needs to be run at the *end*, because the generators are called in
# reverse order, when each downstream transform references `tasks`.
transforms.add(check_if_partners_enabled)
