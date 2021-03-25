# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence


transforms = TransformSequence()


@transforms.add
def sentry(config, tasks):
    """Do transforms specific to github-sync tasks."""
    for task in tasks:
        task["worker"]["env"]["HG_PUSHLOG_URL"] = task["worker"]["env"][
            "HG_PUSHLOG_URL"
        ].format(
            head_repository=config.params["head_repository"],
            head_rev=config.params["head_rev"],
        )
        yield task
