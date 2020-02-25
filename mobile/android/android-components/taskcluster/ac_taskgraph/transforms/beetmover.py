# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

from ..build_config import get_version
from .build import get_nightly_version

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

    def _craft_path_version(version, build_type, nightly_version):
        """Helper function to craft the correct version to bake in the artifacts full
        path section"""
        path_version = "{}{}".format(
            version,
            "-SNAPSHOT" if build_type == "snapshot" else ''
        )
        # XXX: for nightly releases we need to s/X.0.0/X.0.<buildid>/g in version
        # within the destination path. This change is needed to ensure the version folder is set correctly so that
        # maven2/org/mozilla/components/<component>/34.0.0/<component>-34.0.20200212190116-sources.jar
        # becomes ->
        # maven2/org/mozilla/components/<component>/34.0.20200212190116/<component>-34.0.20200212190116-sources.jar
        if build_type == 'nightly':
            path_version = path_version.replace(version, nightly_version)
        return path_version

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
                            version_with_snapshot=_craft_path_version(version,
                                task["attributes"]["build-type"], nightly_version),
                            artifact_file_name=os.path.basename(artifact_path),
                        )
                    ]
                }
                for artifact_path in dep.attributes["artifacts"].values()
            },
            "taskId": {"task-reference": "<{}>".format(dep.kind)},
        } for dep in deps]

        yield task
