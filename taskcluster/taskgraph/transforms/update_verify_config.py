# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

import six.moves.urllib.parse as urlparse

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by
from taskgraph.util.scriptworker import get_release_config
from taskgraph.transforms.task import (
    get_branch_repo,
    get_branch_rev,
)

transforms = TransformSequence()


# The beta regexes do not match point releases.
# In the rare event that we do ship a point
# release to beta, we need to either:
# 1) update these regexes to match that specific version
# 2) pass a second include version that matches that specific version
INCLUDE_VERSION_REGEXES = {
    "beta": r"'^(\d+\.\d+(b\d+)?)$'",
    "nonbeta": r"'^\d+\.\d+(\.\d+)?$'",
    # Same as beta, except excludes 58.0b1 due to issues with it not being able
    # to update to latest
    "devedition_hack": r"'^((?!58\.0b1$)\d+\.\d+(b\d+)?)$'",
    # Same as nonbeta, except for the esr suffix
    "esr": r"'^\d+\.\d+(\.\d+)?esr$'",
    # Previous esr versions, for update testing before we update users to esr91
    "esr91-next": r"'^(52|60|68|78)+\.\d+(\.\d+)?esr$'",
    # Previous Thunderbird major versions. Same idea as esrXX-next, no esr suffix
    "thunderbird91-next": r"'^78\.\d+(\.\d+)?$'",
}

MAR_CHANNEL_ID_OVERRIDE_REGEXES = {
    "beta": r"'^\d+\.\d+(\.\d+)?$$,firefox-mozilla-beta,firefox-mozilla-release'",
}


def ensure_wrapped_singlequote(regexes):
    """Ensure that a regex (from INCLUDE_VERSION_REGEXES or MAR_CHANNEL_ID_OVERRIDE_REGEXES)
    is wrapper in single quotes.
    """
    for name, regex in regexes.items():
        if regex[0] != "'" or regex[-1] != "'":
            raise Exception(
                "Regex {} is invalid: not wrapped with single quotes.\n{}".format(
                    name, regex
                )
            )


ensure_wrapped_singlequote(INCLUDE_VERSION_REGEXES)
ensure_wrapped_singlequote(MAR_CHANNEL_ID_OVERRIDE_REGEXES)


@transforms.add
def add_command(config, tasks):
    keyed_by_args = [
        "channel",
        "archive-prefix",
        "previous-archive-prefix",
        "aus-server",
        "override-certs",
        "include-version",
        "mar-channel-id-override",
        "last-watershed",
    ]
    optional_args = [
        "updater-platform",
    ]

    release_config = get_release_config(config)

    for task in tasks:
        task["description"] = "generate update verify config for {}".format(
            task["attributes"]["build_platform"]
        )

        command = [
            "python",
            "testing/mozharness/scripts/release/update-verify-config-creator.py",
            "--product",
            task["extra"]["product"],
            "--stage-product",
            task["shipping-product"],
            "--app-name",
            task["extra"]["app-name"],
            "--branch-prefix",
            task["extra"]["branch-prefix"],
            "--platform",
            task["extra"]["platform"],
            "--to-version",
            release_config["version"],
            "--to-app-version",
            release_config["appVersion"],
            "--to-build-number",
            str(release_config["build_number"]),
            "--to-buildid",
            config.params["moz_build_date"],
            "--to-revision",
            get_branch_rev(config),
            "--output-file",
            "update-verify.cfg",
        ]

        repo_path = urlparse.urlsplit(get_branch_repo(config)).path.lstrip("/")
        command.extend(["--repo-path", repo_path])

        if release_config.get("partial_versions"):
            for partial in release_config["partial_versions"].split(","):
                command.extend(["--partial-version", partial.split("build")[0]])

        for arg in optional_args:
            if task["extra"].get(arg):
                command.append("--{}".format(arg))
                command.append(task["extra"][arg])

        for arg in keyed_by_args:
            thing = "extra.{}".format(arg)
            resolve_keyed_by(
                task,
                thing,
                item_name=task["name"],
                platform=task["attributes"]["build_platform"],
                **{
                    "release-type": config.params["release_type"],
                    "release-level": config.params.release_level(),
                }
            )
            # ignore things that resolved to null
            if not task["extra"].get(arg):
                continue
            if arg == "include-version":
                task["extra"][arg] = INCLUDE_VERSION_REGEXES[task["extra"][arg]]
            if arg == "mar-channel-id-override":
                task["extra"][arg] = MAR_CHANNEL_ID_OVERRIDE_REGEXES[task["extra"][arg]]

            command.append("--{}".format(arg))
            command.append(task["extra"][arg])

        task["run"].update(
            {
                "using": "mach",
                "mach": " ".join(command),
            }
        )

        yield task
