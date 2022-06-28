# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def sentry(config, tasks):
    """Do transforms specific to github-sync tasks."""
    for task in tasks:
        scopes = [
            scope.format(level=config.params["level"]) for scope in task["scopes"]
        ]
        task["scopes"] = scopes

        env = {
            key: value.format(
                level=config.params["level"],
                head_repository=config.params["head_repository"],
                head_rev=config.params["head_rev"],
            )
            for (key, value) in task["worker"]["env"].items()
        }
        task["worker"]["env"] = env
        yield task
