# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the release-generate-checksums-signing task into task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema
from taskgraph.util.taskcluster import get_artifact_path
from voluptuous import Optional

from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import get_signing_cert_scope

release_generate_checksums_signing_schema = Schema(
    {
        Optional("label"): str,
        Optional("dependencies"): task_description_schema["dependencies"],
        Optional("attributes"): task_description_schema["attributes"],
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("job-from"): task_description_schema["job-from"],
    }
)

transforms = TransformSequence()


@transforms.add
def remote_name(config, jobs):
    for job in jobs:
        if "name" in job:
            del job["name"]
        yield job


transforms.add_validate(release_generate_checksums_signing_schema)


@transforms.add
def make_release_generate_checksums_signing_description(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = copy_attributes_from_dependent_job(dep_job)

        treeherder = job.get("treeherder", {})
        treeherder.setdefault("symbol", "SGenChcks")
        dep_th_platform = (
            dep_job.task.get("extra", {})
            .get("treeherder", {})
            .get("machine", {})
            .get("platform", "")
        )
        treeherder.setdefault("platform", f"{dep_th_platform}/opt")
        treeherder.setdefault("tier", 1)
        treeherder.setdefault("kind", "build")

        job_template = "{}-{}".format(dep_job.label, "signing")
        label = job.get("label", job_template)
        description = "Signing of the overall release-related checksums"

        dependencies = {dep_job.kind: dep_job.label}

        upstream_artifacts = [
            {
                "taskId": {"task-reference": f"<{str(dep_job.kind)}>"},
                "taskType": "build",
                "paths": [
                    get_artifact_path(dep_job, "SHA256SUMS"),
                    get_artifact_path(dep_job, "SHA512SUMS"),
                ],
                "formats": ["autograph_gpg"],
            }
        ]

        signing_cert_scope = get_signing_cert_scope(config)

        task = {
            "label": label,
            "description": description,
            "worker-type": "linux-signing",
            "worker": {
                "implementation": "scriptworker-signing",
                "upstream-artifacts": upstream_artifacts,
                "max-run-time": 3600,
            },
            "scopes": [
                signing_cert_scope,
            ],
            "dependencies": dependencies,
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "treeherder": treeherder,
        }

        yield task
