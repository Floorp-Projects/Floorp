# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency

from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.signed_artifacts import (
    generate_specifications_of_artifacts_to_sign,
)

transforms = TransformSequence()


@transforms.add
def add_signed_routes(config, jobs):
    """Add routes corresponding to the routes of the build task
    this corresponds to, with .signed inserted, for all gecko.v2 routes"""

    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        enable_signing_routes = job.pop("enable-signing-routes", True)

        job["routes"] = []
        if dep_job.attributes.get("shippable") and enable_signing_routes:
            for dep_route in dep_job.task.get("routes", []):
                if not dep_route.startswith("index.gecko.v2"):
                    continue
                branch = dep_route.split(".")[3]
                rest = ".".join(dep_route.split(".")[4:])
                job["routes"].append(f"index.gecko.v2.{branch}.signed.{rest}")

        yield job


@transforms.add
def define_upstream_artifacts(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        upstream_artifact_task = job.pop("upstream-artifact-task", dep_job)

        job.setdefault("attributes", {}).update(
            copy_attributes_from_dependent_job(dep_job)
        )

        artifacts_specifications = generate_specifications_of_artifacts_to_sign(
            config,
            job,
            keep_locale_template=False,
            kind=config.kind,
            dep_kind=upstream_artifact_task.kind,
        )

        task_ref = f"<{upstream_artifact_task.kind}>"
        task_type = "build"
        if "notarization" in upstream_artifact_task.kind:
            task_type = "scriptworker"

        job["upstream-artifacts"] = [
            {
                "taskId": {"task-reference": task_ref},
                "taskType": task_type,
                "paths": spec["artifacts"],
                "formats": spec["formats"],
            }
            for spec in artifacts_specifications
        ]

        yield job
