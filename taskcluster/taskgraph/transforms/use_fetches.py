# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from .base import (
    TransformSequence,
)


transforms = TransformSequence()


def get_attribute(dict, key, attributes, attribute_name):
    '''Get `attribute_name` from the given `attributes` dict, and if there
    is a corresponding value, set `key` in `dict` to that value.'''
    value = attributes.get(attribute_name)
    if value:
        dict[key] = value


@transforms.add
def use_fetches(config, jobs):
    artifacts = {}

    for task in config.kind_dependencies_tasks:
        if task.kind != 'fetch':
            continue

        name = task.label.replace('%s-' % task.kind, '')
        get_attribute(artifacts, name, task.attributes, 'fetch-artifact')

    for job in jobs:
        fetches = job.pop('fetches', [])

        for fetch in fetches:
            if fetch not in artifacts:
                raise Exception('Missing fetch job for %s-%s: %s' % (
                    config.kind, job['name'], fetch))

            if not artifacts[fetch].startswith('public/'):
                raise Exception('non-public artifacts not supported')

        if fetches:
            job.setdefault('dependencies', {}).update(
                ('fetch-%s' % f, 'fetch-%s' % f)
                for f in fetches)

            env = job.setdefault('worker', {}).setdefault('env', {})
            env['MOZ_FETCHES'] = {'task-reference': ' '.join(
                '%s@<fetch-%s>' % (artifacts[f], f)
                for f in fetches)}

        yield job
