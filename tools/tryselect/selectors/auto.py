# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from ..cli import BaseTryParser
from ..push import push_to_try


class AutoParser(BaseTryParser):
    name = 'auto'
    common_groups = ['push']
    task_configs = [
        "artifact",
        "env",
        "chemspill-prio",
        "disable-pgo",
        "strategy",
        "worker-overrides",
    ]


def run(message='{msg}', push=True, closed_tree=False, try_config=None):
    msg = message.format(msg='Tasks automatically selected.')
    try_config = try_config or {}

    # XXX Remove once an intelligent scheduling algorithm is running on
    # autoland by default. This ensures `mach try auto` doesn't run SETA.
    try_config.setdefault('optimize-strategies', 'taskgraph.optimize:bugbug_push_schedules')

    task_config = {
        'version': 2,
        'parameters': {
            'optimize_target_tasks': True,
            'target_tasks_method': 'try_auto',
            'try_mode': 'try_auto',
            'try_task_config': try_config or {},
        }
    }
    return push_to_try('auto', msg, try_task_config=task_config,
                       push=push, closed_tree=closed_tree)
