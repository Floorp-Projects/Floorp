# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the version bump kind
kind.
"""


from gecko_taskgraph.transforms.task import task_description_schema
from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from voluptuous import Optional, Required, Schema

version_bump_description_schema = Schema(
    {
        Required("name"): str,
        Required("worker"): task_description_schema["worker"],  # dict,
        Optional("job-from"): str,
        # treeherder is allowed here to override any defaults we use for beetmover.
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("dependencies"): task_description_schema["dependencies"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Required("worker-type"): task_description_schema["worker-type"],
        Required("description"): task_description_schema["description"],
    }
)

transforms = TransformSequence()
transforms.add_validate(version_bump_description_schema)


@transforms.add
def set_name_and_clear_artifacts(config, tasks):
    for task in tasks:
        task["name"] = task["attributes"]["build-type"]
        task["attributes"]["artifacts"] = {}
        yield task


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("worker.push", "treeherder.symbol"):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                **{
                    "build-type": task["attributes"]["build-type"],
                    "level": config.params["level"],
                }
            )
        yield task
