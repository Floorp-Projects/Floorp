# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re

from gecko_taskgraph.util.scriptworker import (
    generate_beetmover_artifact_map,
    generate_beetmover_upstream_artifacts,
)

_ARTIFACT_ID_PER_PLATFORM = {
    "android-aarch64-opt": "{package}-default-omni-arm64-v8a",
    "android-arm-opt": "{package}-default-omni-armeabi-v7a",
    "android-x86-opt": "{package}-default-omni-x86",
    "android-x86_64-opt": "{package}-default-omni-x86_64",
    "android-geckoview-fat-aar-opt": "{package}-default",
    "android-aarch64-shippable": "{package}{update_channel}-omni-arm64-v8a",
    "android-aarch64-shippable-lite": "{package}{update_channel}-arm64-v8a",
    "android-arm-shippable": "{package}{update_channel}-omni-armeabi-v7a",
    "android-arm-shippable-lite": "{package}{update_channel}-armeabi-v7a",
    "android-x86-shippable": "{package}{update_channel}-omni-x86",
    "android-x86-shippable-lite": "{package}{update_channel}-x86",
    "android-x86_64-shippable": "{package}{update_channel}-omni-x86_64",
    "android-x86_64-shippable-lite": "{package}{update_channel}-x86_64",
    "android-geckoview-fat-aar-shippable": "{package}{update_channel}-omni",
    "android-geckoview-fat-aar-shippable-lite": "{package}{update_channel}",
}


def get_geckoview_artifact_map(config, job):
    return generate_beetmover_artifact_map(
        config,
        job,
        **get_geckoview_template_vars(
            config,
            job["attributes"]["build_platform"],
            job["maven-package"],
            job["attributes"].get("update-channel"),
        ),
    )


def get_geckoview_upstream_artifacts(config, job, package, platform=""):
    if not platform:
        platform = job["attributes"]["build_platform"]
    upstream_artifacts = generate_beetmover_upstream_artifacts(
        config,
        job,
        platform="",
        **get_geckoview_template_vars(
            config, platform, package, job["attributes"].get("update-channel")
        ),
    )
    return [
        {key: value for key, value in upstream_artifact.items() if key != "locale"}
        for upstream_artifact in upstream_artifacts
    ]


def get_geckoview_template_vars(config, platform, package, update_channel):
    version_groups = re.match(r"(\d+).(\d+).*", config.params["version"])
    if version_groups:
        major_version, minor_version = version_groups.groups()

    return {
        "artifact_id": get_geckoview_artifact_id(
            config,
            platform,
            package,
            update_channel,
        ),
        "build_date": config.params["moz_build_date"],
        "major_version": major_version,
        "minor_version": minor_version,
    }


def get_geckoview_artifact_id(config, platform, package, update_channel=None):
    if update_channel == "release":
        update_channel = ""
    elif update_channel is not None:
        update_channel = f"-{update_channel}"
    else:
        # For shippable builds, mozharness defaults to using
        # "nightly-{project}" for the update channel.  For other builds, the
        # update channel is not set, but the value is not substituted.
        update_channel = "-nightly-{}".format(config.params["project"])
    return _ARTIFACT_ID_PER_PLATFORM[platform].format(
        update_channel=update_channel, package=package
    )
