# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def sync_github(config, tasks):
    """Do transforms specific to github-sync tasks."""
    for task in tasks:
        # Add the secret to the scopes, only in m-c.
        # Doing this on any other tree will result in decision task failure
        # because m-c is the only one allowed to have that scope.
        secret = task["secret"]
        if config.params["project"] == "mozilla-central":
            task.setdefault("scopes", [])
            task["scopes"].append("secrets:get:" + secret)
            task["worker"].setdefault("env", {})["GITHUB_SECRET"] = secret
        del task["secret"]
        yield task
