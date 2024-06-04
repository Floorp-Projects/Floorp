# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_android_startup_test_dependencies(config, jobs):
    scheduled_tasks = [
        t
        for t in config.kind_dependencies_tasks.values()
        if t.kind == "android-startup-test"
    ]
    for job in jobs:
        job_build_type = job["attributes"].get("build-type", "")
        matching_tasks = [
            t
            for t in scheduled_tasks
            if job_build_type in t.attributes.get("build-type", "")
        ]
        if matching_tasks:
            job.setdefault("dependencies", {}).update(
                {t.label: t.label for t in matching_tasks}
            )
        yield job
