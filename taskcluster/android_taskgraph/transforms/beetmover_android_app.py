# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.transforms.task import task_description_schema
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by
from voluptuous import ALLOW_EXTRA, Optional, Required, Schema

from android_taskgraph.util.scriptworker import generate_beetmover_artifact_map

logger = logging.getLogger(__name__)

beetmover_description_schema = Schema(
    {
        # unique name to describe this beetmover task, defaults to {dep.label}-beetmover
        Required("name"): str,
        Required("worker"): {"upstream-artifacts": [dict]},
        # treeherder is allowed here to override any defaults we use for beetmover.
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("dependencies"): task_description_schema["dependencies"],
        Optional("bucket-scope"): optionally_keyed_by("level", "build-type", str),
    },
    extra=ALLOW_EXTRA,
)

transforms = TransformSequence()
transforms.add_validate(beetmover_description_schema)


@transforms.add
def make_task_description(config, tasks):
    for task in tasks:
        attributes = task["attributes"]

        label = "beetmover-{}".format(task["name"])
        description = "Beetmover submission for build type '{build_type}'".format(
            build_type=attributes.get("build-type"),
        )

        if task.get("locale"):
            attributes["locale"] = task["locale"]

        resolve_keyed_by(
            task,
            "bucket-scope",
            item_name=task["name"],
            **{
                "build-type": task["attributes"]["build-type"],
                "level": config.params["level"],
            }
        )
        bucket_scope = task.pop("bucket-scope")

        taskdesc = {
            "label": label,
            "description": description,
            "worker-type": "beetmover-android",
            "worker": task["worker"],
            "scopes": [
                bucket_scope,
                "project:releng:beetmover:action:direct-push-to-bucket",
            ],
            "dependencies": task["dependencies"],
            "attributes": attributes,
            "treeherder": task["treeherder"],
        }

        yield taskdesc


_STAGING_PREFIX = "staging-"


def craft_release_properties(config, task):
    params = config.params

    return {
        "app-name": "fenix",  # TODO: Support focus
        "app-version": str(params["version"]),
        "branch": params["project"],
        "build-id": str(params["moz_build_date"]),
        "hash-type": "sha512",
        "platform": "android",
    }


@transforms.add
def make_task_worker(config, tasks):
    for task in tasks:
        locale = task["attributes"].get("locale")
        build_type = task["attributes"]["build-type"]

        task["worker"].update(
            {
                "implementation": "beetmover",
                "release-properties": craft_release_properties(config, task),
                "artifact-map": generate_beetmover_artifact_map(
                    config, task, platform=build_type, locale=locale
                ),
            }
        )

        if locale:
            task["worker"]["locale"] = locale

        yield task
