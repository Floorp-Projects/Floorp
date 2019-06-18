# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import add_suffix, inherit_treeherder_from_dep

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    config_tasks = {}
    for dep in config.kind_dependencies_tasks:
        if 'update-verify-config' in dep.kind or 'update-verify-next-config' in dep.kind:
            config_tasks[dep.name] = dep

    for task in tasks:
        config_task = config_tasks[task['name']]
        total_chunks = task["extra"]["chunks"]
        task['worker'].setdefault('env', {}).update(
            CHANNEL=config_task.task['extra']['channel'],
        )
        task.setdefault('fetches', {})[config_task.label] = [
            "update-verify.cfg",
        ]
        task['treeherder'] = inherit_treeherder_from_dep(task, config_task)

        for this_chunk in range(1, total_chunks+1):
            chunked = deepcopy(task)
            chunked["treeherder"]["symbol"] = add_suffix(
                chunked["treeherder"]["symbol"], this_chunk)
            chunked["label"] = "release-update-verify-{}-{}/{}".format(
                chunked["name"], this_chunk, total_chunks
            )
            if not chunked["worker"].get("env"):
                chunked["worker"]["env"] = {}
            chunked["run"] = {
                'using': 'run-task',
                'command': 'cd /builds/worker/checkouts/gecko && '
                           'tools/update-verify/scripts/chunked-verify.sh '
                           '{} {}'.format(
                               total_chunks,
                               this_chunk,
                           ),
                'sparse-profile': 'update-verify',
            }

            yield chunked
