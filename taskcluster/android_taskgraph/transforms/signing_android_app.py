# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Apply some defaults and minor modifications to the jobs defined in the
APK and AAB signing kinds.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()

PRODUCTION_SIGNING_BUILD_TYPES = [
    "focus-nightly",
    "focus-beta",
    "focus-release",
    "klar-release",
    "fenix-nightly",
    "fenix-beta",
    "fenix-release",
    "fenix-beta-mozillaonline",
    "fenix-release-mozillaonline",
]


@transforms.add
def resolve_keys(config, tasks):
    for task in tasks:
        for key in (
            "index",
            "signing-format",
            "notify",
            "treeherder.platform",
        ):
            resolve_keyed_by(
                task,
                key,
                item_name=task["name"],
                **{
                    "build-type": task["attributes"]["build-type"],
                    "level": config.params["level"],
                },
            )
        yield task


@transforms.add
def set_worker_type(config, tasks):
    for task in tasks:
        worker_type = "linux-depsigning"
        if (
            str(config.params["level"]) == "3"
            and task["attributes"]["build-type"] in PRODUCTION_SIGNING_BUILD_TYPES
        ):
            worker_type = "linux-signing"
        task["worker-type"] = worker_type
        yield task


@transforms.add
def add_signing_cert_scope(config, tasks):
    scope_prefix = config.graph_config["scriptworker"]["scope-prefix"]
    for task in tasks:
        cert = "dep-signing"
        if str(config.params["level"]) == "3":
            if task["attributes"]["build-type"] in ("fenix-beta", "fenix-release"):
                cert = "fennec-production-signing"
            elif task["attributes"]["build-type"] in PRODUCTION_SIGNING_BUILD_TYPES:
                cert = "production-signing"
        task.setdefault("scopes", []).append(f"{scope_prefix}:signing:cert:{cert}")
        yield task


@transforms.add
def set_index_job_name(config, tasks):
    for task in tasks:
        if task.get("index"):
            task["index"]["job-name"] = task["attributes"]["build-type"]
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
