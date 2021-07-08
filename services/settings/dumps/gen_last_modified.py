#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json

import buildconfig
import mozpack.path as mozpath


def get_last_modified(full_path_to_remote_settings_dump_file):
    """
    Get the last_modified for the given file name.
    - File must exist
    - Must be a JSON dictionary with a data list, e.g. `{"data": []}`
    - Every element in `data` should contain a "last_modified" key.
    - The first element must have the highest "last_modified" value.
    """
    with open(full_path_to_remote_settings_dump_file, "r") as f:
        records = json.load(f)["data"]
        assert isinstance(records, list)

    # Various RemoteSettings client code default to 0 when the set of
    # records is empty (-1 is reserved for failures / non-existing files).
    last_modified = 0
    if records:
        # Records in dumps are sorted by last_modified, newest first:
        # https://searchfox.org/mozilla-central/rev/5b3444ad300e244b5af4214212e22bd9e4b7088a/taskcluster/docker/periodic-updates/scripts/periodic_file_updates.sh#304 # NOQA: E501
        last_modified = records[0]["last_modified"]

    assert isinstance(last_modified, int)
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
        "comm/mail",
        "comm/suite",
    )

    remotesettings_dumps = {}

    # For simplicity, let's hardcode the path of the first and (so far) only
    # RemoteSettings dump whose last_modified date is looked up (bug 1717068),
    # i.e. blocklists/addons-bloomfilters.
    # TODO bug 1719560: Replace hardcoded values with something more generic.
    if buildconfig.substs["MOZ_BUILD_APP"] != "mobile/android":
        # Until bug 1639050 is resolved, the dump isn't packaged with Android.
        remotesettings_dumps["blocklists/addons-bloomfilters"] = mozpath.join(
            buildconfig.topsrcdir,
            "services/settings/dumps/blocklists/addons-bloomfilters.json",
        )

    output_dict = {}
    input_files = set()

    for key, input_file in remotesettings_dumps.items():
        input_files.add(input_file)
        output_dict[key] = get_last_modified(input_file)

    json.dump(output_dict, output, sort_keys=True)
    return input_files
