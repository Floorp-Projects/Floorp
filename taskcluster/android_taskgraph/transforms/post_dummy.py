# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def set_name_and_clear_artifacts(config, tasks):
    for task in tasks:
        task["name"] = task["attributes"]["build-type"]
        task["attributes"]["artifacts"] = {}
        yield task
