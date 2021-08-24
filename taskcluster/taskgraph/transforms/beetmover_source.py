# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover-source task to also append `build` as dependency
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def remove_build_dependency_in_beetmover_source(config, jobs):
    for job in jobs:
        # XXX: We delete the build dependency because, unlike the other beetmover
        # tasks, source doesn't depend on any build task at all. This hack should
        # go away when we rewrite beetmover transforms to allow more flexibility in deps
        # Essentially, we should use multi_dep for beetmover.
        for depname in job["dependencies"]:
            if "signing" not in depname:
                del job["dependencies"][depname]
                break
        else:
            raise Exception("Can't find build dep in beetmover source!")

        all_upstream_artifacts = job["worker"]["upstream-artifacts"]
        upstream_artifacts_without_build = [
            upstream_artifact
            for upstream_artifact in all_upstream_artifacts
            if upstream_artifact["taskId"]["task-reference"] != f"<{depname}>"
        ]
        job["worker"]["upstream-artifacts"] = upstream_artifacts_without_build

        yield job
