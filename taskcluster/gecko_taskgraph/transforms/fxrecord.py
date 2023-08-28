# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency

transforms = TransformSequence()


@transforms.add
def fxrecord(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        job["dependencies"] = {dep_job.label: dep_job.label}
        job["treeherder"]["platform"] = dep_job.task["extra"]["treeherder-platform"]
        job["worker"].setdefault("env", {})["FXRECORD_TASK_ID"] = {
            "task-reference": f"<{dep_job.label}>"
        }

        # copy shipping_product from upstream
        product = dep_job.attributes.get(
            "shipping_product", dep_job.task.get("shipping-product")
        )
        if product:
            job.setdefault("shipping-product", product)

        yield job
