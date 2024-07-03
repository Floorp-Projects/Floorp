# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema
from taskgraph.util.treeherder import replace_group
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import (
    generate_beetmover_artifact_map,
    generate_beetmover_upstream_artifacts,
    get_beetmover_action_scope,
    get_beetmover_bucket_scope,
)

transforms = TransformSequence()

beetmover_description_schema = Schema(
    {
        # unique label to describe this beetmover task
        Required("label"): str,
        Required("dependencies"): task_description_schema["dependencies"],
        # treeherder is allowed here to override any defaults we use for beetmover.  See
        # taskcluster/gecko_taskgraph/transforms/task.py for the schema details, and the
        # below transforms for defaults of various values.
        Optional("treeherder"): task_description_schema["treeherder"],
        # locale is passed only for l10n beetmoving
        Optional("locale"): str,
        Required("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("task-from"): task_description_schema["task-from"],
    }
)


@transforms.add
def remove_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(beetmover_description_schema)


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = dep_job.attributes

        treeherder = job.get("treeherder", {})
        treeherder.setdefault(
            "symbol", replace_group(dep_job.task["extra"]["treeherder"]["symbol"], "BM")
        )
        dep_th_platform = (
            dep_job.task.get("extra", {})
            .get("treeherder", {})
            .get("machine", {})
            .get("platform", "")
        )
        treeherder.setdefault("platform", f"{dep_th_platform}/opt")
        treeherder.setdefault(
            "tier", dep_job.task.get("extra", {}).get("treeherder", {}).get("tier", 1)
        )
        treeherder.setdefault("kind", "build")
        label = job["label"]
        description = (
            "Beetmover submission for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get("locale", "en-US"),
                build_platform=attributes.get("build_platform"),
                build_type=attributes.get("build_type"),
            )
        )

        dependencies = {dep_job.kind: dep_job.label}

        # XXX release snap-repackage has a variable number of dependencies, depending on how many
        # "post-beetmover-dummy" jobs there are in the graph.
        if dep_job.kind != "release-snap-repackage" and len(dep_job.dependencies) > 1:
            raise NotImplementedError(
                "Can't beetmove a signing task with multiple dependencies"
            )
        signing_dependencies = dep_job.dependencies
        dependencies.update(signing_dependencies)

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes.update(job.get("attributes", {}))

        if job.get("locale"):
            attributes["locale"] = job["locale"]

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
            "shipping-phase": job["shipping-phase"],
        }

        yield task


def craft_release_properties(config, job):
    params = config.params
    build_platform = job["attributes"]["build_platform"]
    build_platform = build_platform.replace("-shippable", "")
    if build_platform.endswith("-source"):
        build_platform = build_platform.replace("-source", "-release")

    # XXX This should be explicitly set via build attributes or something
    if "android" in job["label"] or "fennec" in job["label"]:
        app_name = "Fennec"
    elif config.graph_config["trust-domain"] == "comm":
        app_name = "Thunderbird"
    else:
        # XXX Even DevEdition is called Firefox
        app_name = "Firefox"

    return {
        "app-name": app_name,
        "app-version": params["app_version"],
        "branch": params["project"],
        "build-id": params["moz_build_date"],
        "hash-type": "sha512",
        "platform": build_platform,
    }


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        valid_beetmover_job = len(job["dependencies"]) == 2 and any(
            ["signing" in j for j in job["dependencies"]]
        )
        # XXX release snap-repackage has a variable number of dependencies, depending on how many
        # "post-beetmover-dummy" jobs there are in the graph.
        if "-snap-" not in job["label"] and not valid_beetmover_job:
            raise NotImplementedError("Beetmover must have two dependencies.")

        locale = job["attributes"].get("locale")
        platform = job["attributes"]["build_platform"]

        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
            "upstream-artifacts": generate_beetmover_upstream_artifacts(
                config, job, platform, locale
            ),
            "artifact-map": generate_beetmover_artifact_map(
                config, job, platform=platform, locale=locale
            ),
        }

        if locale:
            worker["locale"] = locale
        job["worker"] = worker

        yield job
