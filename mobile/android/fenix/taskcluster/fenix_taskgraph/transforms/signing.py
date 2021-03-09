# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the build
kind.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in ("index", "run-on-tasks-for", "worker-type", "worker.signing-type", "signing-format"):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                **{
                    'build-type': task["attributes"]["build-type"],
                    'level': config.params["level"],
                    'tasks-for': config.params["tasks_for"],
                }
            )
        yield task


@transforms.add
def set_signing_attributes(config, tasks):
    for task in tasks:
        task["attributes"]["signed"] = True
        yield task


@transforms.add
def set_signing_format(config, tasks):
    for task in tasks:
        signing_format = task.pop("signing-format")
        for upstream_artifact in task["worker"]["upstream-artifacts"]:
            upstream_artifact["formats"] = [signing_format]
        yield task
