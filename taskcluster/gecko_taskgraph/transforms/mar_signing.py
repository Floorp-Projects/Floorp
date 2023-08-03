# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the {partials,mar}-signing task into an actual task description.
"""

import logging
import os

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.taskcluster import get_artifact_prefix
from taskgraph.util.treeherder import inherit_treeherder_from_dep, join_symbol

from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    sorted_unique_list,
)
from gecko_taskgraph.util.partials import get_partials_artifacts_from_params
from gecko_taskgraph.util.scriptworker import get_signing_cert_scope_per_platform

logger = logging.getLogger(__name__)

SIGNING_FORMATS = {
    "mar-signing-autograph-stage": {
        "target.complete.mar": ["autograph_stage_mar384"],
    },
    "default": {
        "target.complete.mar": ["autograph_hash_only_mar384"],
    },
}

transforms = TransformSequence()


def generate_partials_artifacts(job, release_history, platform, locale=None):
    artifact_prefix = get_artifact_prefix(job)
    if locale:
        artifact_prefix = f"{artifact_prefix}/{locale}"
    else:
        locale = "en-US"

    artifacts = get_partials_artifacts_from_params(release_history, platform, locale)

    upstream_artifacts = [
        {
            "taskId": {"task-reference": "<partials>"},
            "taskType": "partials",
            "paths": [f"{artifact_prefix}/{path}" for path, version in artifacts],
            "formats": ["autograph_hash_only_mar384"],
        }
    ]

    return upstream_artifacts


def generate_complete_artifacts(job, kind):
    upstream_artifacts = []
    if kind not in SIGNING_FORMATS:
        kind = "default"
    for artifact in job.attributes["release_artifacts"]:
        basename = os.path.basename(artifact)
        if basename in SIGNING_FORMATS[kind]:
            upstream_artifacts.append(
                {
                    "taskId": {"task-reference": f"<{job.kind}>"},
                    "taskType": "build",
                    "paths": [artifact],
                    "formats": SIGNING_FORMATS[kind][basename],
                }
            )

    return upstream_artifacts


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        locale = dep_job.attributes.get("locale")

        treeherder = inherit_treeherder_from_dep(job, dep_job)
        treeherder.setdefault(
            "symbol", join_symbol(job.get("treeherder-group", "ms"), locale or "N")
        )

        label = job.get("label", f"{config.kind}-{dep_job.label}")

        dependencies = {dep_job.kind: dep_job.label}
        signing_dependencies = dep_job.dependencies
        # This is so we get the build task etc in our dependencies to
        # have better beetmover support.
        dependencies.update(signing_dependencies)

        attributes = copy_attributes_from_dependent_job(dep_job)
        attributes["required_signoffs"] = sorted_unique_list(
            attributes.get("required_signoffs", []), job.pop("required_signoffs")
        )
        attributes["shipping_phase"] = job["shipping-phase"]
        if locale:
            attributes["locale"] = locale

        build_platform = attributes.get("build_platform")
        if config.kind == "partials-signing":
            upstream_artifacts = generate_partials_artifacts(
                dep_job, config.params["release_history"], build_platform, locale
            )
        else:
            upstream_artifacts = generate_complete_artifacts(dep_job, config.kind)

        is_shippable = job.get(
            "shippable", dep_job.attributes.get("shippable")  # First check current job
        )  # Then dep job for 'shippable'
        signing_cert_scope = get_signing_cert_scope_per_platform(
            build_platform, is_shippable, config
        )

        scopes = [signing_cert_scope]

        task = {
            "label": label,
            "description": "{} {}".format(
                dep_job.description, job["description-suffix"]
            ),
            "worker-type": job.get("worker-type", "linux-signing"),
            "worker": {
                "implementation": "scriptworker-signing",
                "upstream-artifacts": upstream_artifacts,
                "max-run-time": 3600,
            },
            "dependencies": dependencies,
            "attributes": attributes,
            "scopes": scopes,
            "run-on-projects": job.get(
                "run-on-projects", dep_job.attributes.get("run_on_projects")
            ),
            "treeherder": treeherder,
        }

        yield task
