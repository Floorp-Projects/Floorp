# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the github_release
kind.
"""


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("worker.github-project", "worker.is-prerelease", "worker.release-name"):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                **{
                    'build-type': task["attributes"]["build-type"],
                    'level': config.params["level"],
                }
            )
        yield task


@transforms.add
def build_worker_definition(config, tasks):
    for task in tasks:
        worker_definition = {
            # We don't want to upload 10gb of artifacts to the release; let's
            # just create a release as a tag.
            "artifact-map": [],
            "git-tag": config.params["head_tag"],
            "git-revision": config.params["head_rev"],
            "release-name": task["worker"]["release-name"],
            "release-name": task["worker"]["release-name"].format(version=config.params["version"]),
        }

        task["worker"].update(worker_definition)

        yield task
