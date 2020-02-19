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
        for key in ("worker.channel", "worker.commit", "worker.dep"):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                **{
                    'build-type': task["attributes"]["build-type"],
                    'level': config.params["level"],
                }
            )
        yield task


@transforms.add
def build_worker_definition(config, tasks):
    for task in tasks:
        worker_definition = {}
        worker_definition["certificate-alias"] = "{}-{}".format(
            task["worker"]["product"], task["worker"]["channel"]
        )

        build_type = task["attributes"]["build-type"]
        # Fenix production doesn't follow the rule {product}-{channel}
        if build_type == "production":
            worker_definition["certificate-alias"] = "fenix"
        # Neither do Fennec flavored builds
        elif build_type.startswith("fennec-"):
            worker_definition["certificate-alias"] = build_type

        task["worker"].update(worker_definition)

        yield task
