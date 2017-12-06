# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    for task in tasks:
        total_chunks = task["extra"]["chunks"]
        release_config = get_release_config(config)
        release_tag = "{}_{}_RELEASE_RUNTIME".format(
            task["shipping-product"].upper(),
            release_config["version"].replace(".", "_")
        )

        for this_chunk in range(1, total_chunks+1):
            chunked = deepcopy(task)
            chunked["treeherder"]["symbol"] += str(this_chunk)
            chunked["label"] = "release-update-verify-{}-{}/{}".format(
                chunked["name"], this_chunk, total_chunks
            )
            if not chunked["worker"].get("env"):
                chunked["worker"]["env"] = {}
            chunked["worker"]["command"] = [
                "/bin/bash",
                "-c",
                "hg clone $BUILD_TOOLS_REPO tools && cd tools && " +
                "hg up -r {} && cd .. && ".format(
                    release_tag,
                ) +
                "tools/scripts/release/updates/chunked-verify.sh " +
                "UNUSED UNUSED {} {}".format(
                    total_chunks,
                    this_chunk,
                )
            ]
            for thing in ("CHANNEL", "VERIFY_CONFIG", "BUILD_TOOLS_REPO"):
                thing = "worker.env.{}".format(thing)
                resolve_keyed_by(chunked, thing, thing, **config.params)
            yield chunked
