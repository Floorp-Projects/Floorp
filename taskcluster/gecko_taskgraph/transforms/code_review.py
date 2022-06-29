# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add soft dependencies and configuration to code-review tasks.
"""


from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_dependencies(config, jobs):
    for job in jobs:
        job.setdefault("soft-dependencies", [])
        job["soft-dependencies"] += [
            dep_task.label
            for dep_task in config.kind_dependencies_tasks.values()
            if dep_task.attributes.get("code-review") is True
        ]
        yield job


@transforms.add
def add_phabricator_config(config, jobs):
    for job in jobs:
        diff = config.params.get("phabricator_diff")
        if diff is not None:
            code_review = job.setdefault("extra", {}).setdefault("code-review", {})
            code_review["phabricator-diff"] = diff
        yield job
