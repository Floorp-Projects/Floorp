# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Find upstream artifact task.
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def find_upstream_artifact_task(config, jobs):
    for job in jobs:
        dep_job = None
        if job.get("dependent-tasks"):
            dep_labels = [l for l in job["dependent-tasks"].keys()]
            for label in dep_labels:
                if "notarization-part-1" in label:
                    assert (
                        dep_job is None
                    ), "Can't determine whether " "{} or {} is dep_job!".format(
                        dep_job.label, label
                    )
                    dep_job = job["dependent-tasks"][label]
            if dep_job is not None:
                job["upstream-artifact-task"] = dep_job
        yield job
