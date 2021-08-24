# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the snap beetmover kind into an actual task description.
"""


from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def leave_snap_repackage_dependencies_only(config, jobs):
    for job in jobs:
        # XXX: We delete the build dependency because, unlike the other beetmover
        # tasks, source doesn't depend on any build task at all. This hack should
        # go away when we rewrite beetmover transforms to allow more flexibility in deps

        job["dependencies"] = {
            key: value
            for key, value in job["dependencies"].items()
            if key == "release-snap-repackage"
        }

        job["worker"]["upstream-artifacts"] = [
            upstream_artifact
            for upstream_artifact in job["worker"]["upstream-artifacts"]
            if upstream_artifact["taskId"]["task-reference"]
            == "<release-snap-repackage>"
        ]

        yield job


@transforms.add
def set_custom_treeherder_job_name(config, jobs):
    for job in jobs:
        job.get("treeherder", {})["symbol"] = "Snap(BM)"

        yield job
