#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import glob
import json
import os

import buildconfig
import mozpack.path as mozpath


def get_last_modified(full_path_to_remote_settings_dump_file):
    """
    Get the last_modified for the given file name.
    - File must exist
    - Must be a JSON dictionary with a data list and a timestamp,
      e.g. `{"data": [], "timestamp": 42}`
    - Every element in `data` should contain a "last_modified" key.
    - The first element must have the highest "last_modified" value.
    """
    with open(full_path_to_remote_settings_dump_file, "r", encoding="utf-8") as f:
        changeset = json.load(f)

    records = changeset["data"]
    assert isinstance(records, list)
    last_modified = changeset.get("timestamp")
    assert isinstance(
        last_modified, int
    ), f"{full_path_to_remote_settings_dump_file} is missing the timestamp. See Bug 1725660"

    return last_modified


def main(output):
    """
    Generates a JSON file that maps "bucket/collection" to the last_modified
    value within.

    Returns a set of the file locations of the recorded RemoteSettings dumps,
    so that the build backend can invoke this script again when needed.

    The validity of the JSON file is verified through unit tests at
    services/settings/test/unit/test_remote_settings_dump_lastmodified.js
    """
    # The following build targets currently use RemoteSettings dumps:
    # Firefox               https://searchfox.org/mozilla-central/rev/94d6086481754e154b6f042820afab6bc9900a30/browser/installer/package-manifest.in#281-285        # NOQA: E501
    # Firefox for Android   https://searchfox.org/mozilla-central/rev/94d6086481754e154b6f042820afab6bc9900a30/mobile/android/installer/package-manifest.in#88-91   # NOQA: E501
    # Thunderbird           https://searchfox.org/comm-central/rev/89f957706bbda77e5f34e64e117e7ce121bb5d83/mail/installer/package-manifest.in#280-285              # NOQA: E501
    # SeaMonkey             https://searchfox.org/comm-central/rev/89f957706bbda77e5f34e64e117e7ce121bb5d83/suite/installer/package-manifest.in#307-309             # NOQA: E501
    assert buildconfig.substs["MOZ_BUILD_APP"] in (
        "browser",
        "mobile/android",
        "mobile/ios",
        "comm/mail",
        "comm/suite",
    ), (
        "Cannot determine location of Remote Settings "
        f"dumps for platform {buildconfig.substs['MOZ_BUILD_APP']}"
    )

    dumps_locations = []
    if buildconfig.substs["MOZ_BUILD_APP"] == "browser":
        dumps_locations += ["services/settings/dumps/"]
    elif buildconfig.substs["MOZ_BUILD_APP"] == "mobile/android":
        dumps_locations += ["services/settings/dumps/"]
    elif buildconfig.substs["MOZ_BUILD_APP"] == "mobile/ios":
        dumps_locations += ["services/settings/dumps/"]
    elif buildconfig.substs["MOZ_BUILD_APP"] == "comm/mail":
        dumps_locations += ["services/settings/dumps/"]
        dumps_locations += ["comm/mail/app/settings/dumps/"]
    elif buildconfig.substs["MOZ_BUILD_APP"] == "comm/suite":
        dumps_locations += ["services/settings/dumps/"]

    remotesettings_dumps = {}
    for dumps_location in dumps_locations:
        dumps_root_dir = mozpath.join(buildconfig.topsrcdir, *dumps_location.split("/"))
        for path in glob.iglob(mozpath.join(dumps_root_dir, "*", "*.json")):
            folder, filename = os.path.split(path)
            bucket = os.path.basename(folder)
            collection, _ = os.path.splitext(filename)
            remotesettings_dumps[f"{bucket}/{collection}"] = path

    output_dict = {}
    input_files = set()

    for key, input_file in remotesettings_dumps.items():
        input_files.add(input_file)
        output_dict[key] = get_last_modified(input_file)

    json.dump(output_dict, output, sort_keys=True)
    return input_files
