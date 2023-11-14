# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform release-beetmover-source-checksums into an actual task description.
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

beetmover_checksums_description_schema = Schema(
    {
        Optional("label"): str,
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("locale"): str,
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("dependencies"): task_description_schema["dependencies"],
        Optional("job-from"): task_description_schema["job-from"],
    }
)

transforms = TransformSequence()


@transforms.add
def remove_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(beetmover_checksums_description_schema)


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = dep_job.attributes

        treeherder = job.get("treeherder", {})
        treeherder.setdefault("symbol", "BMcss(N)")
        dep_th_platform = (
            dep_job.task.get("extra", {})
            .get("treeherder", {})
            .get("machine", {})
            .get("platform", "")
        )
        treeherder.setdefault("platform", f"{dep_th_platform}/opt")
        treeherder.setdefault("tier", 1)
        treeherder.setdefault("kind", "build")

        label = job["label"]
        build_platform = attributes.get("build_platform")

        description = "Beetmover submission of checksums for source file"

        extra = {}
        if "devedition" in build_platform:
            extra["product"] = "devedition"
        else:
            extra["product"] = "firefox"

        dependencies = {dep_job.kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith("beetmover"):
                dependencies[k] = v

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get("attributes", {}))

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
            "extra": extra,
        }

        if "shipping-phase" in job:
            task["shipping-phase"] = job["shipping-phase"]

        if "shipping-product" in job:
            task["shipping-product"] = job["shipping-product"]

        yield task


@transforms.add
def make_beetmover_checksums_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = len(job["dependencies"]) == 2
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover checksums must have two dependencies.")

        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        refs = {
            "beetmover": None,
            "signing": None,
        }
        for dependency in job["dependencies"].keys():
            if dependency.startswith("beetmover"):
                refs["beetmover"] = f"<{dependency}>"
            else:
                refs["signing"] = f"<{dependency}>"
        if None in refs.values():
            raise NotImplementedError(
                "Beetmover checksums must have a beetmover and signing dependency!"
            )

        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_beetmover_upstream_artifacts(
                config, job, platform, locale
            ),
            "artifact-map": generate_beetmover_artifact_map(
                config, job, platform=platform
            ),
        }

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
