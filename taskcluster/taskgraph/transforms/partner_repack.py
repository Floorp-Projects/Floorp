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
from taskgraph.util.partners import (
    check_if_partners_enabled,
    get_partner_url_config,
)


transforms = TransformSequence()


@transforms.add
def populate_repack_manifests_url(config, tasks):
    for task in tasks:
        partner_url_config = get_partner_url_config(config.params, config.graph_config)

        for k in partner_url_config:
            if config.kind.startswith(k):
                task['worker'].setdefault('env', {})['REPACK_MANIFESTS_URL'] = \
                    partner_url_config[k]
                break
        else:
            raise Exception("Can't find partner REPACK_MANIFESTS_URL")

        for property in ("limit-locales", ):
            property = "extra.{}".format(property)
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
    all_locales = set()
    for partner_class in config.params['release_partner_config'].values():
        for partner in partner_class.values():
            for sub_partner in partner.values():
                all_locales.update(sub_partner.get('locales', []))
    for task in tasks:
        # add the MOZHARNESS_OPTIONS, eg version=61.0, build-number=1, platform=win64
        task['run']['options'] = [
            'version={}'.format(release_config['version']),
            'build-number={}'.format(release_config['build_number']),
            'platform={}'.format(task['attributes']['build_platform'].split('-')[0]),
        ]
        if task['extra']['limit-locales']:
            for locale in all_locales:
                task['run']['options'].append('limit-locale={}'.format(locale))
        if 'partner' in config.kind and config.params['release_partners']:
            for partner in config.params['release_partners']:
                task['run']['options'].append('p={}'.format(partner))

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
