# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Take the base iris task definition and generate all of the actual test chunks
for all combinations of test categories and test platforms.
"""

from __future__ import absolute_import, print_function, unicode_literals

from copy import deepcopy

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def make_iris_tasks(config, jobs):
    # Each platform will get a copy of the test categories
    platforms = config.config.get("iris-build-platforms")

    # The fields needing to be resolve_keyed_by'd
    fields = [
        "dependencies.build",
        "fetches.build",
        "run.command",
        "run-on-projects",
        "treeherder.platform",
        "worker.docker-image",
        "worker.artifacts",
        "worker.env.PATH",
        "worker.max-run-time",
        "worker-type",
    ]

    for job in jobs:
        for platform in platforms:
            # Make platform-specific clones of each iris task
            clone = deepcopy(job)

            basename = clone["name"]
            clone["description"] = clone["description"].format(basename)
            clone["name"] = clone["name"] + "-" + platform

            # resolve_keyed_by picks the correct values based on
            # the `by-platform` keys in the task definitions
            for field in fields:
                resolve_keyed_by(
                    clone,
                    field,
                    clone["name"],
                    **{
                        "platform": platform,
                    }
                )

            # iris uses this to select the tests to run in this chunk
            clone["worker"]["env"]["CURRENT_TEST_DIR"] = basename

            # Clean up some entries when they aren't needed
            if clone["worker"]["docker-image"] is None:
                del clone["worker"]["docker-image"]
            if clone["worker"]["env"]["PATH"] is None:
                del clone["worker"]["env"]["PATH"]

            yield clone


@transforms.add
def fill_email_data(config, tasks):
    format_kwargs = {
        "head_rev": config.params["head_rev"],
        "project": config.params["project"],
        "th_root": "https://treeherder.mozilla.org/#/",
        "tiers": "&tier=1%2C2%2C3",
    }

    for task in tasks:
        format_kwargs["task_name"] = task["name"]
        format_kwargs["filterstring"] = "&searchStr=iris%20{}".format(task["name"])
        format_kwargs["chunk"] = task["worker"]["env"]["CURRENT_TEST_DIR"]

        resolve_keyed_by(
            task,
            "notify.email",
            item_name=task["name"],
            **{
                "project": config.params["project"],
            }
        )

        email = task["notify"].get("email")
        if email:
            email["link"]["href"] = email["link"]["href"].format(**format_kwargs)
            email["subject"] = email["subject"].format(**format_kwargs)

        yield task


@transforms.add
def add_notify_email(config, tasks):
    for task in tasks:
        notify = task.pop("notify", {})
        email_config = notify.get("email")
        if email_config:
            extra = task.setdefault("extra", {})
            notify = extra.setdefault("notify", {})
            notify["email"] = {
                "subject": email_config["subject"],
                "content": email_config["message"],
                "link": email_config.get("link", None),
            }

            routes = task.setdefault("routes", [])
            routes.extend(
                [
                    "notify.email.{}.on-{}".format(address, reason)
                    for address in email_config["emails"]
                    for reason in email_config["on-reasons"]
                ]
            )

        yield task
