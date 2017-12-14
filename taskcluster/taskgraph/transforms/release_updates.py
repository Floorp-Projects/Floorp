# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the update generation task into an actual task description.
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

        real_task = deepcopy(task)
        real_task.setdefault("worker", {}).setdefault("properties", {})

        real_task["worker"]["properties"]["version"] = release_config["version"]
        real_task["worker"]["properties"]["appVersion"] = release_config["appVersion"]
        real_task["worker"]["properties"]["build_number"] = release_config["build_number"]
        real_task["worker"]["properties"]["partial_versions"] = release_config.get(
            "partial_versions", ""
        )

        for thing in ("generate_bz2_blob", "balrog_api_root", "channels", "repo_path"):
            thing = "worker.properties.{}".format(thing)
            resolve_keyed_by(real_task, thing, thing, **config.params)

        # Non-RC builds from mozilla-release shouldn't use the beta channel.
        if config.params.get('project') == 'mozilla-release':
            if config.params.get('desktop_release_type') != "rc":
                real_task["worker"]["properties"]["channels"] = \
                    real_task["worker"]["properties"]["channels"].replace("beta,", "")

        yield real_task
