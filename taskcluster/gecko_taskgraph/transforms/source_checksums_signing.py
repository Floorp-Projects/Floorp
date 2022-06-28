# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the checksums signing task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from voluptuous import Optional

from gecko_taskgraph.loader.single_dep import schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import get_signing_cert_scope
from gecko_taskgraph.transforms.task import task_description_schema

checksums_signing_description_schema = schema.extend(
    {
        Optional("label"): str,
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("shipping-product"): task_description_schema["shipping-product"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
    }
)

transforms = TransformSequence()
transforms.add_validate(checksums_signing_description_schema)


@transforms.add
def make_checksums_signing_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]
        attributes = dep_job.attributes

        treeherder = job.get("treeherder", {})
        treeherder.setdefault("symbol", "css(N)")
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
        description = "Signing of release-source checksums file"
        dependencies = {"beetmover": dep_job.label}

        attributes = copy_attributes_from_dependent_job(dep_job)

        upstream_artifacts = [
            {
                "taskId": {"task-reference": "<beetmover>"},
                "taskType": "beetmover",
                "paths": [
                    "public/target-source.checksums",
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
