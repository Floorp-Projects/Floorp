# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def add_pr_number(config, tasks):
    for task in tasks:
        include_pr = task.pop("include-pull-request-number")
        if include_pr and config.params["pull_request_number"]:
            task["worker"]["env"]["PULL_REQUEST_NUMBER"] = str(config.params["pull_request_number"])

        yield task
