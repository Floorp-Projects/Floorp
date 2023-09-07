# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def set_name_and_clear_artifacts(config, tasks):
    for task in tasks:
        task["name"] = task["attributes"]["build-type"]
        task["attributes"]["artifacts"] = {}
        yield task


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        resolve_keyed_by(
            task,
            "treeherder.symbol",
            item_name=task["name"],
            **{
                "build-type": task["attributes"]["build-type"],
            }
        )
        yield task
