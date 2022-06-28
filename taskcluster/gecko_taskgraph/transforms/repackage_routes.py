# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add indexes to repackage kinds
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_indexes(config, jobs):
    for job in jobs:
        repackage_type = job["attributes"].get("repackage_type")
        if repackage_type and job["attributes"]["build_type"] != "debug":
            build_platform = job["attributes"]["build_platform"]
            job_name = f"{build_platform}-{repackage_type}"
            product = job.get("index", {}).get("product", "firefox")
            index_type = "generic"
            if job["attributes"].get("shippable") and job["attributes"].get("locale"):
                index_type = "shippable-l10n"
            if job["attributes"].get("shippable"):
                index_type = "shippable"
            if job["attributes"].get("locale"):
                index_type = "l10n"
            job["index"] = {
                "job-name": job_name,
                "product": product,
                "type": index_type,
            }

        yield job
