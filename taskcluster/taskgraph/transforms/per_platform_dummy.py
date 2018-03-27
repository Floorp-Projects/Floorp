# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def one_task_per_product_and_platform(config, jobs):
    unique_products_and_platforms = set()
    for job in jobs:
        dep_task = job["dependent-task"]
        if 'dependent-task' in job:
            del job['dependent-task']
        product = dep_task.attributes.get("shipping_product")
        platform = dep_task.attributes.get("build_platform")
        if (product, platform) not in unique_products_and_platforms:
            job.setdefault("attributes", {})
            job["attributes"]["shipping_product"] = product
            job["attributes"]["build_platform"] = platform
            job["name"] = "{}-{}".format(product, platform)
            yield job
            unique_products_and_platforms.add((product, platform))
