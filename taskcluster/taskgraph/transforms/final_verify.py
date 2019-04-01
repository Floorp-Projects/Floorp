# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    for task in tasks:
        if not task["worker"].get("env"):
            task["worker"]["env"] = {}

        final_verify_configs = []
        for upstream in task.get("dependencies", {}).keys():
            if 'update-verify-config' in upstream:
                final_verify_configs.append(
                    "<{}/public/build/update-verify.cfg>".format(upstream),
                )
        task['run'] = {
            'using': 'run-task',
            'command': {
                'artifact-reference': 'cd /builds/worker/checkouts/gecko && '
                                      'tools/update-verify/release/final-verification.sh '
                                      + ' '.join(final_verify_configs),
            },
            'sparse-profile': 'update-verify',
        }
        yield task
