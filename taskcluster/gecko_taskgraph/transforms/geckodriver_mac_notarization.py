# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage signing task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.schema import Schema
from voluptuous import Optional

from gecko_taskgraph.transforms.task import task_description_schema
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job
from gecko_taskgraph.util.scriptworker import add_scope_prefix

repackage_signing_description_schema = Schema(
    {
        Optional("label"): str,
        Optional("treeherder"): task_description_schema["treeherder"],
        Optional("shipping-phase"): task_description_schema["shipping-phase"],
        Optional("worker"): task_description_schema["worker"],
        Optional("worker-type"): task_description_schema["worker-type"],
        Optional("job-from"): task_description_schema["job-from"],
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


transforms.add_validate(repackage_signing_description_schema)


@transforms.add
def geckodriver_mac_notarization(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        attributes = copy_attributes_from_dependent_job(dep_job)
        treeherder = job.get("treeherder", {})
        dep_treeherder = dep_job.task.get("extra", {}).get("treeherder", {})
        treeherder.setdefault(
            "platform", dep_job.task.get("extra", {}).get("treeherder-platform")
        )
        treeherder.setdefault("tier", dep_treeherder.get("tier", 1))
        treeherder.setdefault("kind", "build")

        dependencies = {dep_job.kind: dep_job.label}

        description = "Mac notarization - Geckodriver for build '{}'".format(
            attributes.get("build_platform"),
        )

        build_platform = dep_job.attributes.get("build_platform")

        scopes = [add_scope_prefix(config, "signing:cert:release-apple-notarization")]

        platform = build_platform.rsplit("-", 1)[0]

        task = {
            "label": job["label"],
            "description": description,
            "worker-type": job["worker-type"],
            "worker": job["worker"],
            "scopes": scopes,
            "dependencies": dependencies,
            "attributes": attributes,
            "treeherder": treeherder,
            "run-on-projects": ["mozilla-central"],
            "index": {"product": "geckodriver", "job-name": f"{platform}-notarized"},
        }
        yield task
