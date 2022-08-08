# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from ..build_config import get_version
from .build import get_nightly_version, craft_path_version

transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("treeherder.job-symbol", "worker.bucket", "worker.beetmover-application-name"):
            resolve_keyed_by(
                task, key,
                item_name=task["name"],
                **{
                    "build-type": task["attributes"]["build-type"],
                    "level": config.params["level"],
                }
            )
        yield task


@transforms.add
def set_artifact_map(config, tasks):


    version = get_version()
    nightly_version = get_nightly_version(config, version)

    for task in tasks:
        maven_destination = task.pop("maven-destination")
        deps = task.pop("dependent-tasks").values()
        task["worker"]["artifact-map"] = [{
            "paths": {
                artifact_path: {
                    "destinations": [
                        maven_destination.format(
                            component=task["attributes"]["component"],
                            version=craft_path_version(version,
                                task["attributes"]["build-type"], nightly_version),
                            artifact_file_name=os.path.basename(artifact_path),
                        )
                    ]
                }
                for artifact_path in dep.attributes["artifacts"].values()
            },
            "taskId": {"task-reference": f"<{dep.kind}>"},
        } for dep in deps]

        yield task


@transforms.add
def add_version(config, tasks):
    version = get_version()
    nightly_version = get_nightly_version(config, version)

    for task in tasks:
        task["worker"]["version"] = craft_path_version(
            version,
            task["attributes"]["build-type"],
            nightly_version
        )
        yield task
