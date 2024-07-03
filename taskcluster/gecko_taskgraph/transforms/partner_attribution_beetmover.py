# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema
from taskgraph.util.taskcluster import get_artifact_prefix
from voluptuous import Optional, Required

from gecko_taskgraph.transforms.beetmover import craft_release_properties
from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
)
from gecko_taskgraph.util.partners import (
    apply_partner_priority,
)
from gecko_taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_beetmover_bucket_scope,
)

beetmover_description_schema = Schema(
    {
        # from the loader:
        Optional("task-from"): str,
        Optional("name"): str,
        # from the from_deps transforms:
        Optional("attributes"): task_description_schema["attributes"],
        Optional("dependencies"): task_description_schema["dependencies"],
        # depname is used in taskref's to identify the taskID of the unsigned things
        Required("depname", default="build"): str,
        # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
        Optional("label"): str,
        Required("partner-path"): str,
        Optional("extra"): object,
        Required("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("priority"): task_description_schema["priority"],
    }
)

transforms = TransformSequence()
transforms.add_validate(beetmover_description_schema)
transforms.add(apply_partner_priority)


@transforms.add
def populate_scopes_and_upstream_artifacts(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        upstream_artifacts = dep_job.attributes["release_artifacts"]
        attribution_task_ref = "<{}>".format(dep_job.label)
        prefix = get_artifact_prefix(dep_job)
        artifacts = []
        for artifact in upstream_artifacts:
            partner, sub_partner, platform, locale, _ = artifact.replace(
                prefix + "/", ""
            ).split("/", 4)
            artifacts.append((artifact, partner, sub_partner, platform, locale))

        action_scope = add_scope_prefix(config, "beetmover:action:push-to-partner")
        bucket_scope = get_beetmover_bucket_scope(config)
        repl_dict = {
            "build_number": config.params["build_number"],
            "release_partner_build_number": config.params[
                "release_partner_build_number"
            ],
            "version": config.params["version"],
            "partner": "{partner}",  # we'll replace these later, per artifact
            "subpartner": "{subpartner}",
            "platform": "{platform}",
            "locale": "{locale}",
        }
        job["scopes"] = [bucket_scope, action_scope]

        partner_path = job["partner-path"].format(**repl_dict)
        job.setdefault("worker", {})[
            "upstream-artifacts"
        ] = generate_upstream_artifacts(attribution_task_ref, artifacts, partner_path)

        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = dep_job.attributes
        build_platform = attributes.get("build_platform")
        if not build_platform:
            raise Exception("Cannot find build platform!")

        label = config.kind
        description = "Beetmover for partner attribution"
        attributes = copy_attributes_from_dependent_job(dep_job)

        task = {
            "label": label,
            "description": description,
            "dependencies": {dep_job.kind: dep_job.label},
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "shipping-phase": job["shipping-phase"],
            "shipping-product": job.get("shipping-product"),
            "worker": job["worker"],
            "scopes": job["scopes"],
        }
        # we may have reduced the priority for partner jobs, otherwise task.py will set it
        if job.get("priority"):
            task["priority"] = job["priority"]

        yield task


def generate_upstream_artifacts(attribution_task, artifacts, partner_path):
    upstream_artifacts = []
    for artifact, partner, subpartner, platform, locale in artifacts:
        upstream_artifacts.append(
            {
                "taskId": {"task-reference": attribution_task},
                "taskType": "repackage",
                "paths": [artifact],
                "locale": partner_path.format(
                    partner=partner,
                    subpartner=subpartner,
                    platform=platform,
                    locale=locale,
                ),
            }
        )

    if not upstream_artifacts:
        raise Exception("Couldn't find any upstream artifacts.")

    return upstream_artifacts


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        job["worker-type"] = "beetmover"
        worker = {
            "implementation": "beetmover",
            "release-properties": craft_release_properties(config, job),
        }
        job["worker"].update(worker)

        yield job
