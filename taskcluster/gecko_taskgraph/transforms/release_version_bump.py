# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the update generation task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def handle_keyed_by(config, tasks):
    """Resolve fields that can be keyed by platform, etc."""
    default_fields = [
        "worker.push",
        "worker.bump-files",
        "worker-type",
    ]
    for task in tasks:
        fields = default_fields[:]
        for additional_field in (
            "l10n-bump-info",
            "source-repo",
            "dontbuild",
            "ignore-closed-tree",
        ):
            if additional_field in task["worker"]:
                fields.append(f"worker.{additional_field}")
        for field in fields:
            resolve_keyed_by(
                task,
                field,
                item_name=task["name"],
                **{
                    "project": config.params["project"],
                    "release-type": config.params["release_type"],
                },
            )
        yield task
