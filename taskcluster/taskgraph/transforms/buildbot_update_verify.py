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
        release_config = get_release_config(config)
        total_chunks = task["extra"]["chunks"]
        platform = task["attributes"]["build_platform"]
        # We git rid of -devedition (if it exists here) because we're
        # only using this in the buildername, and buildbot doesn't use
        # -devedition.
        platform = platform.replace('-devedition', '')
        product = task["shipping-product"]
        buildername = "release-{branch}_" + product + "_" + platform + \
            "_update_verify"
        release_tag = "{}_{}_RELEASE_RUNTIME".format(
            task["shipping-product"].upper(),
            release_config["version"].replace(".", "_")
        )

        for this_chunk in range(1, total_chunks+1):
            chunked = deepcopy(task)
            chunked["scopes"] = [
                "project:releng:buildbot-bridge:builder-name:{}".format(
                    # In scopes, "branch" is called "project"
                    buildername.replace("branch", "project")
                )
            ]
            chunked["label"] = "release-update-verify-{}-{}/{}".format(
                chunked["name"], this_chunk, total_chunks
            )
            chunked["run"]["buildername"] = buildername
            chunked.setdefault("worker", {}).setdefault("properties", {})
            chunked["worker"]["properties"]["script_repo_revision"] = \
                release_tag
            chunked["worker"]["properties"]["THIS_CHUNK"] = str(this_chunk)
            chunked["worker"]["properties"]["TOTAL_CHUNKS"] = str(total_chunks)
            for thing in ("CHANNEL", "VERIFY_CONFIG"):
                thing = "worker.properties.{}".format(thing)
                resolve_keyed_by(chunked, thing, thing, **config.params)
            yield chunked
