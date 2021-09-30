# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""


from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.treeherder import join_symbol

transforms = TransformSequence()


@transforms.add
def make_beetmover_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]

        locale = dep_job.attributes.get("locale")
        if not locale:
            yield job
            continue

        group = "BMR"

        # add the locale code
        symbol = locale

        treeherder = {
            "symbol": join_symbol(group, symbol),
        }

        beet_description = {
            "label": job["label"],
            "primary-dependency": dep_job,
            "dependent-tasks": job["dependent-tasks"],
            "attributes": job["attributes"],
            "treeherder": treeherder,
            "locale": locale,
            "shipping-phase": job["shipping-phase"],
            "shipping-product": job["shipping-product"],
        }
        yield beet_description
