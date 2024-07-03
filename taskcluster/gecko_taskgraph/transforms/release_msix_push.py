# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-msix-push kind into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import Schema, optionally_keyed_by, resolve_keyed_by
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.scriptworker import add_scope_prefix

push_msix_description_schema = Schema(
    {
        Required("name"): str,
        Required("task-from"): task_description_schema["task-from"],
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
transforms.add_validate(push_msix_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
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
            "worker.publish-mode",
            item_name=job["name"],
            **{"release-type": config.params["release_type"]},
        )
        resolve_keyed_by(
            job,
            "worker-type",
            item_name=job["name"],
            **{"release-level": release_level(config.params["project"])},
        )
        if release_level(config.params["project"]) == "production":
            job.setdefault("scopes", []).append(
                add_scope_prefix(
                    config,
                    "microsoftstore:{}".format(job["worker"]["channel"]),
                )
            )

        # Override shipping-phase for release: push to the Store early to
        # allow time for certification.
        if job["worker"]["publish-mode"] == "Manual":
            job["shipping-phase"] = "promote"

        yield job


def generate_upstream_artifacts(dependencies):
    return [
        {
            "taskId": {"task-reference": f"<{task_kind}>"},
            "taskType": "build",
            "paths": ["public/build/target.store.msix"],
        }
        for task_kind in dependencies.keys()
    ]
