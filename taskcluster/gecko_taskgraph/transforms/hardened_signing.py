# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_primary_dependency
from taskgraph.util.keyed_by import evaluate_keyed_by

from gecko_taskgraph.util.attributes import release_level

transforms = TransformSequence()


@transforms.add
def add_hardened_sign_config(config, jobs):
    for job in jobs:
        if (
            "signing" not in config.kind
            or "macosx" not in job["attributes"]["build_platform"]
        ):
            yield job
            continue

        dep_job = get_primary_dependency(config, job)
        project_level = release_level(config.params["project"])
        is_shippable = dep_job.attributes.get("shippable", False)
        hardened_signing_type = "developer"

        # If project is production AND shippable build, then use production entitlements
        #  Note: debug builds require developer entitlements
        if project_level == "production" and is_shippable:
            hardened_signing_type = "production"

        evaluated = evaluate_keyed_by(
            config.graph_config["mac-signing"]["hardened-sign-config"],
            "hardened-sign-config",
            {"hardened-signing-type": hardened_signing_type},
        )
        if type(evaluated) != list:
            raise Exception("hardened-sign-config must be a list")

        for sign_cfg in evaluated:
            if "entitlements" in sign_cfg and not sign_cfg.get(
                "entitlements", ""
            ).startswith("http"):
                sign_cfg["entitlements"] = config.params.file_url(
                    sign_cfg["entitlements"]
                )

        job["worker"]["hardened-sign-config"] = evaluated
        job["worker"]["mac-behavior"] = "mac_sign_and_pkg_hardened"
        yield job
