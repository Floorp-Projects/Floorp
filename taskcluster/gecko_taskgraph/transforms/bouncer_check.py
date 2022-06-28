# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
from pipes import quote as shell_quote

from taskgraph.util.schema import resolve_keyed_by

from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.attributes import release_level
from gecko_taskgraph.util.scriptworker import get_release_config

import logging

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def add_command(config, jobs):
    for job in jobs:
        command = [
            "python",
            "testing/mozharness/scripts/release/bouncer_check.py",
        ]
        job["run"].update(
            {
                "using": "mach",
                "mach": command,
            }
        )
        yield job


@transforms.add
def add_previous_versions(config, jobs):
    release_config = get_release_config(config)
    if not release_config.get("partial_versions"):
        for job in jobs:
            yield job
    else:
        extra_params = []
        for partial in release_config["partial_versions"].split(","):
            extra_params.append(
                "--previous-version={}".format(partial.split("build")[0].strip())
            )

        for job in jobs:
            job["run"]["mach"].extend(extra_params)
            yield job


@transforms.add
def handle_keyed_by(config, jobs):
    """Resolve fields that can be keyed by project, etc."""
    fields = [
        "run.config",
        "run.product-field",
        "run.extra-config",
    ]

    release_config = get_release_config(config)
    version = release_config["version"]

    for job in jobs:
        for field in fields:
            resolve_keyed_by(
                item=job,
                field=field,
                item_name=job["name"],
                **{
                    "project": config.params["project"],
                    "release-level": release_level(config.params["project"]),
                    "release-type": config.params["release_type"],
                },
            )

        for cfg in job["run"]["config"]:
            job["run"]["mach"].extend(["--config", cfg])

        if config.kind == "cron-bouncer-check":
            job["run"]["mach"].extend(
                [
                    "--product-field={}".format(job["run"]["product-field"]),
                    "--products-url={}".format(job["run"]["products-url"]),
                ]
            )
            del job["run"]["product-field"]
            del job["run"]["products-url"]
        elif config.kind == "release-bouncer-check":
            job["run"]["mach"].append(f"--version={version}")

        del job["run"]["config"]

        if "extra-config" in job["run"]:
            env = job["worker"].setdefault("env", {})
            env["EXTRA_MOZHARNESS_CONFIG"] = json.dumps(
                job["run"]["extra-config"], sort_keys=True
            )
            del job["run"]["extra-config"]

        yield job


@transforms.add
def command_to_string(config, jobs):
    """Convert command to string to make it work properly with run-task"""
    for job in jobs:
        job["run"]["mach"] = " ".join(map(shell_quote, job["run"]["mach"]))
        yield job
