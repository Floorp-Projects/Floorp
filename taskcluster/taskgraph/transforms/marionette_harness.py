# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Set dynamic task description properties of the marionette-harness task.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def setup_task(config, tasks):
    for task in tasks:
        del task['name']
        task['label'] = 'marionette-harness'
        env = task['worker'].setdefault('env', {})
        env.update({
            'GECKO_BASE_REPOSITORY': config.params['base_repository'],
            'GECKO_HEAD_REF': config.params['head_rev'],
            'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
            'GECKO_HEAD_REV': config.params['head_rev'],
            'MOZ_BUILD_DATE': config.params['moz_build_date'],
            'MOZ_SCM_LEVEL': config.params['level'],
        })

        task['worker']['caches'] = [{
            'type': 'persistent',
            'name': 'level-{}-{}-tc-vcs'.format(
                config.params['level'], config.params['project']),
            'mount-point': "/home/worker/.tc-vcs",
        }]

        yield task
