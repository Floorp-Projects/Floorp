# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    for task in tasks:
        release_config = get_release_config(config)
        release_tag = "{}_{}_RELEASE_RUNTIME".format(
            task["shipping-product"].upper(),
            release_config["version"].replace(".", "_")
        )

        if not task["worker"].get("env"):
            task["worker"]["env"] = {}
        task["worker"]["command"] = [
            "/bin/bash",
            "-c",
            "hg clone $BUILD_TOOLS_REPO tools && cd tools &&" +
            "hg up -r {} && cd release && ".format(
                release_tag,
            ) +
            "./final-verification.sh $FINAL_VERIFY_CONFIGS"
        ]
        for thing in ("FINAL_VERIFY_CONFIGS", "BUILD_TOOLS_REPO"):
            thing = "worker.env.{}".format(thing)
            resolve_keyed_by(task, thing, thing, **config.params)
        yield task
