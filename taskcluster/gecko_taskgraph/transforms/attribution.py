# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def stub_installer(config, jobs):
    """Not all windows builds come with a stub installer (only win32, and not
    on esr), so conditionally add it here based on our dependency's
    stub-installer attribute."""
    for job in jobs:
        dep_name, dep_label = next(iter(job["dependencies"].items()))
        dep_task = config.kind_dependencies_tasks[dep_label]
        if dep_task.attributes.get("stub-installer"):
            locale = job["attributes"].get("locale")
            if locale:
                artifact = f"{locale}/target.stub-installer.exe"
            else:
                artifact = "target.stub-installer.exe"
            job["fetches"][dep_name].append(artifact)
            job["run"]["command"] += [
                "--input",
                "/builds/worker/fetches/target.stub-installer.exe",
            ]
            job["attributes"]["release_artifacts"].append(
                "public/build/target.stub-installer.exe"
            )
        yield job
