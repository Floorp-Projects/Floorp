# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the github_release
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

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
            "artifact-map": _build_artifact_map(task),
            "git-tag": config.params["head_tag"],
            "git-revision": config.params["head_rev"],
            "release-name": task["worker"]["release-name"].format(version=config.params["version"]),
        }

        task["worker"].update(worker_definition)

        yield task

def _build_artifact_map(task):
    artifact_map = []
    github_names_per_path = {
        apk_metadata["name"]: apk_metadata["github-name"]
        for apk_metadata in task["attributes"]["apks"].values()
    }

    for upstream_artifact_metadata in task["worker"]["upstream-artifacts"]:
        artifacts = {"paths": {}, "taskId": upstream_artifact_metadata["taskId"]}
        for path in upstream_artifact_metadata["paths"]:
            artifacts["paths"][path] = {
                "destinations": [github_names_per_path[path]]
            }

        artifact_map.append(artifacts)

    return artifact_map
