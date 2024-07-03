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

beetmover_checksums_description_schema = Schema(
    {
        Optional("label"): str,
        Optional("extra"): object,
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("task-from"): task_description_schema["task-from"],
        Optional("attributes"): task_description_schema["attributes"],
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


transforms.add_validate(beetmover_checksums_description_schema)


@transforms.add
def make_beetmover_checksums_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = dep_job.attributes
        build_platform = attributes.get("build_platform")
        if not build_platform:
            raise Exception("Cannot find build platform!")
        repack_id = dep_job.task.get("extra", {}).get("repack_id")
        if not repack_id:
            raise Exception("Cannot find repack id!")

        label = dep_job.label.replace("beetmover-", "beetmover-checksums-")
        description = (
            "Beetmove checksums for repack_id '{repack_id}' for build '"
            "{build_platform}/{build_type}'".format(
                repack_id=repack_id,
                build_platform=build_platform,
                build_type=attributes.get("build_type"),
            )
        )

        extra = {}
        extra["partner_path"] = dep_job.task["payload"]["upstreamArtifacts"][0][
            "locale"
        ]
        extra["repack_id"] = repack_id

        dependencies = {dep_job.kind: dep_job.label}
        for k, v in dep_job.dependencies.items():
            if k.startswith("beetmover"):
                dependencies[k] = v

        attributes = copy_attributes_from_dependent_job(dep_job)

        task = {
            "label": label,
            "description": description,
            "worker-type": "{}/{}".format(
                dep_job.task["provisionerId"],
                dep_job.task["workerType"],
            ),
            "scopes": dep_job.task["scopes"],
            "dependencies": dependencies,
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "extra": extra,
        }

        if "shipping-phase" in job:
            task["shipping-phase"] = job["shipping-phase"]

        if "shipping-product" in job:
            task["shipping-product"] = job["shipping-product"]

        yield task


def generate_upstream_artifacts(refs, partner_path):
    # Until bug 1331141 is fixed, if you are adding any new artifacts here that
    # need to be transfered to S3, please be aware you also need to follow-up
    # with a beetmover patch in https://github.com/mozilla-releng/beetmoverscript/.
    # See example in bug 1348286
    common_paths = [
        "public/target.checksums",
    ]

    upstream_artifacts = [
        {
            "taskId": {"task-reference": refs["beetmover"]},
            "taskType": "signing",
            "paths": common_paths,
            "locale": f"beetmover-checksums/{partner_path}",
        }
    ]

    return upstream_artifacts


@transforms.add
def make_beetmover_checksums_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = len(job["dependencies"]) == 1
        if not valid_beetmover_job:
            raise NotImplementedError("Beetmover checksums must have one dependency.")

        refs = {
            "beetmover": None,
        }
        for dependency in job["dependencies"].keys():
            if dependency.endswith("beetmover"):
                refs["beetmover"] = f"<{dependency}>"
        if None in refs.values():
            raise NotImplementedError(
                "Beetmover checksums must have a beetmover dependency!"
            )

        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_upstream_artifacts(
                refs,
                job["extra"]["partner_path"],
            ),
        }

        job["worker"] = worker

        yield job
