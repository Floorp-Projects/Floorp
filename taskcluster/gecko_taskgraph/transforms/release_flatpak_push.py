# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-flatpak-push kind into an actual task description.
"""


from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by, Schema
from gecko_taskgraph.util.scriptworker import add_scope_prefix

from voluptuous import Optional, Required

push_flatpak_description_schema = Schema(
    {
        Required("name"): str,
        Required("job-from"): task_description_schema["job-from"],
        Required("dependencies"): task_description_schema["dependencies"],
        Required("description"): task_description_schema["description"],
        Required("treeherder"): task_description_schema["treeherder"],
        Required("run-on-projects"): task_description_schema["run-on-projects"],
        Required("worker-type"): optionally_keyed_by("release-level", str),
        Required("worker"): object,
        Optional("scopes"): [str],
        Required("shipping-phase"): task_description_schema["shipping-phase"],
        Required("shipping-product"): task_description_schema["shipping-product"],
        Optional("extra"): task_description_schema["extra"],
        Optional("attributes"): task_description_schema["attributes"],
    }
)

transforms = TransformSequence()
transforms.add_validate(push_flatpak_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        if len(job["dependencies"]) != 1:
            raise Exception("Exactly 1 dependency is required")

        job["worker"]["upstream-artifacts"] = generate_upstream_artifacts(
            job["dependencies"]
        )

        resolve_keyed_by(
            job,
            "worker.channel",
            item_name=job["name"],
            **{"release-type": config.params["release_type"]},
        )
        resolve_keyed_by(
            job,
            "worker-type",
            item_name=job["name"],
            **{"release-level": config.params.release_level()},
        )
        if config.params.release_level() == "production":
            job.setdefault("scopes", []).append(
                add_scope_prefix(
                    config,
                    "flathub:firefox:{}".format(job["worker"]["channel"]),
                )
            )

        yield job


def generate_upstream_artifacts(dependencies):
    return [
        {
            "taskId": {"task-reference": f"<{task_kind}>"},
            "taskType": "build",
            "paths": ["public/build/target.flatpak.tar.xz"],
        }
        for task_kind in dependencies.keys()
    ]
