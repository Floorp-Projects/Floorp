# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job

transforms = TransformSequence()


@transforms.add
def one_task_per_product_and_platform(config, jobs):
    unique_products_and_platforms = set()
    for job in jobs:
        dep_task = job["primary-dependency"]
        if 'primary-dependency' in job:
            del job['primary-dependency']
        product = dep_task.attributes.get("shipping_product")
        platform = dep_task.attributes.get("build_platform")
        release_type = dep_task.attributes.get("release-type")
        if (product, platform, release_type) not in unique_products_and_platforms:
            attributes = copy_attributes_from_dependent_job(dep_task)
            attributes.update(job.get('attributes', {}))
            job['attributes'] = attributes
            job["name"] = "-".join(filter(None, (product, platform, release_type)))
            yield job
            unique_products_and_platforms.add((product, platform, release_type))
