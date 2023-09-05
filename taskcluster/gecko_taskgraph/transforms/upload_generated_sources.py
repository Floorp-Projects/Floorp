# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the upload-generated-files task description template,
taskcluster/ci/upload-generated-sources/kind.yml, into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency

transforms = TransformSequence()


@transforms.add
def add_task_info(config, jobs):
    for job in jobs:
        dep_task = get_primary_dependency(config, job)
        assert dep_task

        # Add a dependency on the build task.
        job["dependencies"] = {"build": dep_task.label}
        # Label the job to match the build task it's uploading from.
        job["label"] = dep_task.label.replace("build-", "upload-generated-sources-")
        # Copy over some bits of metdata from the build task.
        dep_th = dep_task.task["extra"]["treeherder"]
        job.setdefault("attributes", {})
        job["attributes"]["build_platform"] = dep_task.attributes.get("build_platform")
        if dep_task.attributes.get("shippable"):
            job["attributes"]["shippable"] = True
        plat = "{}/{}".format(
            dep_th["machine"]["platform"], dep_task.attributes.get("build_type")
        )
        job["treeherder"]["platform"] = plat
        job["treeherder"]["tier"] = dep_th["tier"]
        if dep_th["symbol"] != "N":
            job["treeherder"]["symbol"] = "Ugs{}".format(dep_th["symbol"])
        job["run-on-projects"] = dep_task.attributes.get("run_on_projects")
        job["optimization"] = dep_task.optimization
        job["shipping-product"] = dep_task.attributes.get("shipping_product")

        yield job
