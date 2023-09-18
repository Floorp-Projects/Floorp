# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the signing task into an actual task description.
"""

from taskgraph.transforms.base import TransformSequence
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
        release_lvl = release_level(config.params["project"])

        if isinstance(config.graph_config["mac-signing"]["hardened-sign-config"], dict):
            evaluated = evaluate_keyed_by(
                config.graph_config["mac-signing"]["hardened-sign-config"],
                "hardened-sign-config",
                {"release-level": release_lvl},
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

            config.graph_config["mac-signing"]["hardened-sign-config"] = evaluated

        job["worker"]["hardened-sign-config"] = config.graph_config["mac-signing"][
            "hardened-sign-config"
        ]
        job["worker"]["mac-behavior"] = "mac_sign_and_pkg_hardened"
        yield job
