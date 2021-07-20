# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import re

from taskgraph.util.scriptworker import generate_beetmover_upstream_artifacts


_ARTIFACT_ID_PER_PLATFORM = {
    "android-aarch64-opt": "geckoview-default-arm64-v8a",
    "android-arm-opt": "geckoview-default-armeabi-v7a",
    "android-x86-opt": "geckoview-default-x86",
    "android-x86_64-opt": "geckoview-default-x86_64",
    "android-geckoview-fat-aar-opt": "geckoview-default",
    "android-aarch64-shippable": "geckoview{update_channel}-arm64-v8a",
    "android-arm-shippable": "geckoview{update_channel}-armeabi-v7a",
    "android-x86-shippable": "geckoview{update_channel}-x86",
    "android-x86_64-shippable": "geckoview{update_channel}-x86_64",
    "android-geckoview-fat-aar-shippable": "geckoview{update_channel}",
}


def get_geckoview_upstream_artifacts(config, job, platform=""):
    if not platform:
        platform = job["attributes"]["build_platform"]
    upstream_artifacts = generate_beetmover_upstream_artifacts(
        config,
        job,
        platform="",
        **get_geckoview_template_vars(
            config, platform, job["attributes"].get("update-channel")
        )
    )
    return [
        {key: value for key, value in upstream_artifact.items() if key != "locale"}
        for upstream_artifact in upstream_artifacts
    ]


def get_geckoview_template_vars(config, platform, update_channel):
    version_groups = re.match(r"(\d+).(\d+).*", config.params["version"])
    if version_groups:
        major_version, minor_version = version_groups.groups()

    return {
        "artifact_id": get_geckoview_artifact_id(
            config,
            platform,
            update_channel,
        ),
        "build_date": config.params["moz_build_date"],
        "major_version": major_version,
        "minor_version": minor_version,
    }


def get_geckoview_artifact_id(config, platform, update_channel=None):
    if update_channel == "release":
        update_channel = ""
    elif update_channel is not None:
        update_channel = "-{}".format(update_channel)
    else:
        # For shippable builds, mozharness defaults to using
        # "nightly-{project}" for the update channel.  For other builds, the
        # update channel is not set, but the value is not substituted.
        update_channel = "-nightly-{}".format(config.params["project"])
    return _ARTIFACT_ID_PER_PLATFORM[platform].format(update_channel=update_channel)
