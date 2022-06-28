# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover-push-to-release task into a task description.
"""

from taskgraph.util.schema import Schema, taskref_or_string
from voluptuous import Required, Optional

from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.scriptworker import (
    get_beetmover_bucket_scope,
    add_scope_prefix,
)
from gecko_taskgraph.transforms.task import task_description_schema

beetmover_push_to_release_description_schema = Schema(
    {
        Required("name"): str,
        Required("product"): str,
        Required("treeherder-platform"): str,
        Optional("attributes"): {str: object},
        Optional("job-from"): task_description_schema["job-from"],
        Optional("run"): {str: object},
        Optional("run-on-projects"): task_description_schema["run-on-projects"],
        Optional("dependencies"): {str: taskref_or_string},
        Optional("index"): {str: str},
        Optional("routes"): [str],
        Required("shipping-phase"): task_description_schema["shipping-phase"],
        Required("shipping-product"): task_description_schema["shipping-product"],
        Optional("extra"): task_description_schema["extra"],
    }
)


transforms = TransformSequence()
transforms.add_validate(beetmover_push_to_release_description_schema)


@transforms.add
def make_beetmover_push_to_release_description(config, jobs):
    for job in jobs:
        treeherder = job.get("treeherder", {})
        treeherder.setdefault("symbol", "Rel(BM-C)")
        treeherder.setdefault("tier", 1)
        treeherder.setdefault("kind", "build")
        treeherder.setdefault("platform", job["treeherder-platform"])

        label = job["name"]
        description = "Beetmover push to release for '{product}'".format(
            product=job["product"]
        )

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = add_scope_prefix(config, "beetmover:action:push-to-releases")

        task = {
            "label": label,
            "description": description,
            "worker-type": "beetmover",
            "scopes": [bucket_scope, action_scope],
            "product": job["product"],
            "dependencies": job["dependencies"],
            "attributes": job.get("attributes", {}),
            "run-on-projects": job.get("run-on-projects"),
            "treeherder": treeherder,
            "shipping-phase": job.get("shipping-phase", "push"),
            "shipping-product": job.get("shipping-product"),
            "routes": job.get("routes", []),
            "extra": job.get("extra", {}),
        }

        yield task


@transforms.add
def make_beetmover_push_to_release_worker(config, jobs):
    for job in jobs:
        worker = {
            "implementation": "beetmover-push-to-release",
            "product": job["product"],
        }
        job["worker"] = worker
        del job["product"]

        yield job
