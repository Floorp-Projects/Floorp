# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency

from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job

transforms = TransformSequence()


@transforms.add
def one_task_per_product_and_platform(config, jobs):
    unique_products_and_platforms = set()
    for job in jobs:
        dep_task = get_primary_dependency(config, job)
        assert dep_task

        product = dep_task.attributes.get("shipping_product")
        platform = dep_task.attributes.get("build_platform")
        if (product, platform) not in unique_products_and_platforms:
            attr_denylist = ("l10n_chunk", "locale", "artifact_map", "artifact_prefix")
            attributes = copy_attributes_from_dependent_job(
                dep_task, denylist=attr_denylist
            )
            attributes.update(job.get("attributes", {}))
            job["attributes"] = attributes
            job["name"] = f"{product}-{platform}"
            job["shipping-product"] = product
            del job["dependencies"]
            yield job
            unique_products_and_platforms.add((product, platform))
