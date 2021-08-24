# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_job_try_name(config, jobs):
    """
    For a task which is governed by `-j` in try syntax, set the `job_try_name`
    attribute based on the job name.
    """
    for job in jobs:
        job.setdefault("attributes", {}).setdefault("job_try_name", job["name"])
        yield job
