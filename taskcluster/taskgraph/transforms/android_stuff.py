# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Set dynamic task description properties of the android stuff.  Temporary!
"""

from __future__ import absolute_import, print_function, unicode_literals

import time
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def setup_task(config, tasks):
    for task in tasks:
        task['label'] = task['name']
        env = task['worker'].setdefault('env', {})
        env.update({
            'GECKO_BASE_REPOSITORY': config.params['base_repository'],
            'GECKO_HEAD_REF': config.params['head_rev'],
            'GECKO_HEAD_REPOSITORY': config.params['head_repository'],
            'GECKO_HEAD_REV': config.params['head_rev'],
            'MOZ_BUILD_DATE': time.strftime("%Y%m%d%H%M%S",
                                            time.gmtime(config.params['pushdate'])),
            'MOZ_SCM_LEVEL': config.params['level'],
            'MH_BRANCH': config.params['project'],
        })

        task['worker'].setdefault('caches', []).append({
            'type': 'persistent',
            'name': 'level-{}-{}-tc-vcs'.format(
                config.params['level'], config.params['project']),
            'mount-point': "/home/worker/.tc-vcs",
        })

        if int(config.params['level']) > 1:
            task['worker'].setdefault('caches', []).append({
                'type': 'persistent',
                'name': 'level-{}-{}-build-{}-workspace'.format(
                    config.params['level'], config.params['project'], task['name']),
                'mount-point': "/home/worker/workspace",
            })

        del task['name']
        yield task
