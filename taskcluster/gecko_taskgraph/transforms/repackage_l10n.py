# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the repackage task into an actual task description.
"""


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.copy import deepcopy
from taskgraph.util.dependencies import get_primary_dependency

transforms = TransformSequence()


@transforms.add
def split_locales(config, jobs):
    for job in jobs:
        dep_job = get_primary_dependency(config, job)
        assert dep_job

        for locale in dep_job.attributes.get("chunk_locales", []):
            locale_job = deepcopy(job)  # don't overwrite dict values here
            treeherder = locale_job.setdefault("treeherder", {})
            treeherder_group = locale_job.pop("treeherder-group")
            treeherder["symbol"] = f"{treeherder_group}({locale})"
            locale_job["locale"] = locale
            yield locale_job
