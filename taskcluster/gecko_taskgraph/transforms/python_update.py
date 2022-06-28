# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repo-update task into an actual task description.
"""

from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        env = task["worker"].setdefault("env", {})
        env["BRANCH"] = config.params["project"]
        for envvar in env:
            resolve_keyed_by(env, envvar, envvar, **config.params)

        for envvar in list(env.keys()):
            if not env.get(envvar):
                del env[envvar]
        yield task
