# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import logging

from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.base import TransformSequence

logger = logging.getLogger(__name__)


transforms = TransformSequence()


@transforms.add
def make_task_worker(config, jobs):
    for job in jobs:
        resolve_keyed_by(
            job, "worker-type", item_name=job["name"], project=config.params["project"]
        )
        resolve_keyed_by(
            job, "scopes", item_name=job["name"], project=config.params["project"]
        )
        resolve_keyed_by(
            job,
            "bouncer-products",
            item_name=job["name"],
            project=config.params["project"],
        )

        job["worker"]["bouncer-products"] = job["bouncer-products"]

        del job["bouncer-products"]
        yield job
