# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.taskcluster import get_taskcluster_artifact_prefix

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    for task in tasks:
        if not task["worker"].get("env"):
            task["worker"]["env"] = {}

        final_verify_configs = []
        for upstream in task.get("dependencies", {}).keys():
            if 'update-verify-config' in upstream:
                final_verify_configs.append(
                    "{}update-verify.cfg".format(
                        get_taskcluster_artifact_prefix(task, "<{}>".format(upstream))
                    )
                )
        task["worker"]["command"] = [
            "/bin/bash",
            "-c",
            {
                "task-reference": "hg clone $BUILD_TOOLS_REPO tools && cd tools/release && " +
                                  "./final-verification.sh " +
                                  " ".join(final_verify_configs)
            }
        ]
        for thing in ("BUILD_TOOLS_REPO",):
            thing = "worker.env.{}".format(thing)
            resolve_keyed_by(task, thing, thing, **config.params)
        yield task
