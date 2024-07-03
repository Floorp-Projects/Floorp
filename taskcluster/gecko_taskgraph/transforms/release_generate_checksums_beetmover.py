# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the `release-generate-checksums-beetmover` task to also append `build` as dependency
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema
from voluptuous import Optional

from gecko_taskgraph.transforms.beetmover import craft_release_properties
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import (
    generate_beetmover_artifact_map,
    generate_beetmover_upstream_artifacts,
    get_beetmover_action_scope,
    get_beetmover_bucket_scope,
)

transforms = TransformSequence()


release_generate_checksums_beetmover_schema = Schema(
    {
        # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
        Optional("label"): str,
        # treeherder is allowed here to override any defaults we use for beetmover.  See
        # taskcluster/gecko_taskgraph/transforms/task.py for the schema details, and the
        # below transforms for defaults of various values.
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("task-from"): task_description_schema["task-from"],
        Optional("dependencies"): task_description_schema["dependencies"],
    }
)

transforms = TransformSequence()


@transforms.add
def remove_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(release_generate_checksums_beetmover_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get("attributes", {}))

        treeherder = job.get("treeherder", {})
        treeherder.setdefault("symbol", "BM-SGenChcks")
        dep_th_platform = (
            dep_job.task.get("extra", {})
            .get("treeherder", {})
            .get("machine", {})
            .get("platform", "")
        )
        treeherder.setdefault("platform", f"{dep_th_platform}/opt")
        treeherder.setdefault("tier", 1)
        treeherder.setdefault("kind", "build")

        job_template = f"{dep_job.label}"
        label = job_template.replace("signing", "beetmover")

        description = "Transfer *SUMS and *SUMMARY checksums file to S3."

        # first dependency is the signing task for the *SUMS files
        dependencies = {dep_job.kind: dep_job.label}

        if len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't beetmove a signing task with multiple dependencies"
            )
        # update the dependencies with the dependencies of the signing task
        dependencies.update(dep_job.dependencies)

        bucket_scope = get_beetmover_bucket_scope(config)
        action_scope = get_beetmover_action_scope(config)

        task = {
            "label": label,
            "description": description,
            "worker-type": "beetmover",
            "scopes": [bucket_scope, action_scope],
            "dependencies": dependencies,
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "treeherder": treeherder,
            "shipping-phase": "promote",
        }

        yield task


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = len(job["dependencies"]) == 2 and any(
            ["signing" in j for j in job["dependencies"]]
        )
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover must have two dependencies.")

        platform = job["attributes"]["build_platform"]
        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_beetmover_upstream_artifacts(
                config, job, platform=None, locale=None
            ),
            "artifact-map": generate_beetmover_artifact_map(
                config, job, platform=platform
            ),
        }

        job["worker"] = worker

        yield job
