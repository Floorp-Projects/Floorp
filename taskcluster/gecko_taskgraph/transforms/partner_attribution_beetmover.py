# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from collections import defaultdict
from copy import deepcopy

from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.schema import optionally_keyed_by, resolve_keyed_by
from voluptuous import Any, Required, Optional

from gecko_taskgraph.loader.single_dep import schema
from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.transforms.beetmover import craft_release_properties
from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    release_level,
)
from gecko_taskgraph.util.partners import (
    get_partner_config_by_kind,
    apply_partner_priority,
)
from gecko_taskgraph.util.scriptworker import (
    add_scope_prefix,
    get_beetmover_bucket_scope,
)
from gecko_taskgraph.transforms.task import task_description_schema


beetmover_description_schema = schema.extend(
    {
        # depname is used in taskref's to identify the taskID of the unsigned things
        Required("depname", default="build"): str,
        # unique label to describe this beetmover task, defaults to {dep.label}-beetmover
        Optional("label"): str,
        Required("partner-bucket-scope"): optionally_keyed_by("release-level", str),
        Required("partner-public-path"): Any(None, str),
        Required("partner-private-path"): Any(None, str),
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
def resolve_keys(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job,
            "partner-bucket-scope",
            item_name=job["label"],
            **{"release-level": release_level(config.params["project"])},
        )
        yield job


@transforms.add
def split_public_and_private(config, jobs):
    # we need to separate private vs public destinations because beetmover supports one
    # in a single task. Only use a single task for each type though.
    partner_config = get_partner_config_by_kind(config, config.kind)
    for job in jobs:
        upstream_artifacts = job["primary-dependency"].attributes.get(
            "release_artifacts"
        )
        attribution_task_ref = "<{}>".format(job["primary-dependency"].label)
        prefix = get_artifact_prefix(job["primary-dependency"])
        artifacts = defaultdict(list)
        for artifact in upstream_artifacts:
            partner, sub_partner, platform, locale, _ = artifact.replace(
                prefix + "/", ""
            ).split("/", 4)
            destination = "private"
            this_config = [
                p
                for p in partner_config["configs"]
                if (p["campaign"] == partner and p["content"] == sub_partner)
            ]
            if this_config[0].get("upload_to_candidates"):
                destination = "public"
            artifacts[destination].append(
                (artifact, partner, sub_partner, platform, locale)
            )

        action_scope = add_scope_prefix(config, "beetmover:action:push-to-partner")
        public_bucket_scope = get_beetmover_bucket_scope(config)
        partner_bucket_scope = add_scope_prefix(config, job["partner-bucket-scope"])
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
        for destination, destination_artifacts in artifacts.items():
            this_job = deepcopy(job)

            if destination == "public":
                this_job["scopes"] = [public_bucket_scope, action_scope]
                this_job["partner_public"] = True
            else:
                this_job["scopes"] = [partner_bucket_scope, action_scope]
                this_job["partner_public"] = False

            partner_path_key = "partner-{destination}-path".format(
                destination=destination
            )
            partner_path = this_job[partner_path_key].format(**repl_dict)
            this_job.setdefault("worker", {})[
                "upstream-artifacts"
            ] = generate_upstream_artifacts(
                attribution_task_ref, destination_artifacts, partner_path
            )

            yield this_job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]

        attributes = dep_job.attributes
        build_platform = attributes.get("build_platform")
        if not build_platform:
            raise Exception("Cannot find build platform!")

        label = config.kind
        description = "Beetmover for partner attribution"
        if job["partner_public"]:
            label = f"{label}-public"
            description = f"{description} public"
        else:
            label = f"{label}-private"
            description = f"{description} private"
        attributes = copy_attributes_from_dependent_job(dep_job)

        task = {
            "label": label,
            "description": description,
            "dependencies": {dep_job.kind: dep_job.label},
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "shipping-phase": job["shipping-phase"],
            "shipping-product": job.get("shipping-product"),
            "partner_public": job["partner_public"],
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
            "partner-public": job["partner_public"],
        }
        job["worker"].update(worker)
        del job["partner_public"]

        yield job
