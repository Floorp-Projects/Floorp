# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the per-locale balrog task into an actual task description.
"""


from gecko_taskgraph.loader.single_dep import schema
from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.schema import (
    optionally_keyed_by,
    resolve_keyed_by,
)
from gecko_taskgraph.util.treeherder import replace_group
from gecko_taskgraph.transforms.task import task_description_schema
from voluptuous import Optional


balrog_description_schema = schema.extend(
    {
        # unique label to describe this balrog task, defaults to balrog-{dep.label}
        Optional("label"): str,
        Optional(
            "update-no-wnp",
            description="Whether the parallel `-No-WNP` blob should be updated as well.",
        ): optionally_keyed_by("release-type", bool),
        # treeherder is allowed here to override any defaults we use for beetmover.  See
        # taskcluster/gecko_taskgraph/transforms/task.py for the schema details, and the
        # below transforms for defaults of various values.
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("attributes"): task_description_schema["attributes"],
        # Shipping product / phase
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
    }
)


transforms = TransformSequence()
transforms.add_validate(balrog_description_schema)


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by platform, etc."""
    fields = [
        "update-no-wnp",
    ]
    for job in jobs:
        label = job.get("dependent-task", object).__dict__.get("label", "?no-label?")
        for field in fields:
            resolve_keyed_by(
                item=job,
                field=field,
                item_name=label,
                **{
                    "project": config.params["project"],
                    "release-type": config.params["release_type"],
                },
            )
        yield job


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]

        treeherder = job.get("treeherder", {})
        treeherder.setdefault("symbol", "c-Up(N)")
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

        attributes = copy_attributes_from_dependent_job(dep_job)

        treeherder_job_symbol = dep_job.task["extra"]["treeherder"]["symbol"]
        treeherder["symbol"] = replace_group(treeherder_job_symbol, "c-Up")

        if dep_job.attributes.get("locale"):
            attributes["locale"] = dep_job.attributes.get("locale")

        label = job["label"]

        description = (
            "Balrog submission for locale '{locale}' for build '"
            "{build_platform}/{build_type}'".format(
                locale=attributes.get("locale", "en-US"),
                build_platform=attributes.get("build_platform"),
                build_type=attributes.get("build_type"),
            )
        )

        upstream_artifacts = [
            {
                "taskId": {"task-reference": "<beetmover>"},
                "taskType": "beetmover",
                "paths": ["public/manifest.json"],
            }
        ]

        dependencies = {"beetmover": dep_job.label}
        for kind_dep in config.kind_dependencies_tasks.values():
            if (
                kind_dep.kind == "startup-test"
                and kind_dep.attributes["build_platform"]
                == attributes.get("build_platform")
                and kind_dep.attributes["build_type"] == attributes.get("build_type")
                and kind_dep.attributes.get("shipping_product")
                == job.get("shipping-product")
            ):
                dependencies["startup-test"] = kind_dep.label

        task = {
            "label": label,
            "description": description,
            "worker-type": "balrog",
            "worker": {
                "implementation": "balrog",
                "upstream-artifacts": upstream_artifacts,
                "balrog-action": "v2-submit-locale",
                "suffixes": ["", "-No-WNP"] if job.get("update-no-wnp") else [""],
            },
            "dependencies": dependencies,
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "treeherder": treeherder,
            "shipping-phase": job.get("shipping-phase", "promote"),
            "shipping-product": job.get("shipping-product"),
        }

        yield task
