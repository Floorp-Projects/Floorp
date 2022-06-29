# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Add dependencies to dummy macosx64 tasks.
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_dependencies(config, jobs):
    for job in jobs:
        dependencies = {}

        platform = job.get("attributes", {}).get("build_platform")
        if not platform:
            continue
        arm = platform.replace("macosx64", "macosx64-aarch64")
        intel = platform.replace("macosx64", "macosx64-x64")
        for dep_task in config.kind_dependencies_tasks.values():
            # Weed out unwanted tasks.
            if dep_task.attributes.get("build_platform"):
                if dep_task.attributes["build_platform"] not in (platform, arm, intel):
                    continue
            # Add matching tasks to deps
            dependencies[dep_task.label] = dep_task.label
            # Pick one task to copy run-on-projects from
            if (
                dep_task.kind == "build"
                and dep_task.attributes["build_platform"] == platform
            ):
                job["run-on-projects"] = dep_task.attributes.get("run_on_projects")

        job.setdefault("dependencies", {}).update(dependencies)
        job["if-dependencies"] = list(dependencies)

        yield job
