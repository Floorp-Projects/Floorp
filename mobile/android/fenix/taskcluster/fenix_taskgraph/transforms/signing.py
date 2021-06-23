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
        for key in ("run-on-tasks-for", "signing-format", "notify"):
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
def set_worker_type(config, tasks):
    for task in tasks:
        worker_type = "dep-signing"
        if (
            str(config.params["level"]) == "3"
            and task["attributes"]["build-type"]
            in ("nightly", "beta", "release", "android-test-nightly", "beta-mozillaonline", "release-mozillaonline")
            and config.params["tasks_for"] in ("cron", "github-release", "action")
        ):
            worker_type = "signing"
        task["worker-type"] = worker_type
        yield task


@transforms.add
def set_signing_type(config, tasks):
    for task in tasks:
        signing_type = "dep-signing"
        if (
            str(config.params["level"]) == "3"
            and config.params["tasks_for"] in ("cron", "github-release", "action")
        ):
            if task["attributes"]["build-type"] in ("beta", "release"):
                signing_type = "fennec-production-signing"
            elif task["attributes"]["build-type"] in ("nightly", "android-test-nightly", "beta-mozillaonline", "release-mozillaonline"):
                signing_type = "production-signing"
        task.setdefault("worker", {})["signing-type"] = signing_type
        yield task


@transforms.add
def set_index(config, tasks):
    for task in tasks:
        index = {}
        if (
            config.params["tasks_for"] in ("cron", "github-release", "action")
            and task["attributes"]["build-type"]
            in ("nightly", "debut", "nightly-simulation", "beta", "release")
        ):
            index["type"] = "signing"
        task["index"] = index
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


@transforms.add
def format_email(config, tasks):
    version = config.params["version"]

    for task in tasks:
        if "notify" in task:
            email = task["notify"].get("email")
            if email:
                email["subject"] = email["subject"].format(version=version)
                email["content"] = email["content"].format(version=version)

        yield task
