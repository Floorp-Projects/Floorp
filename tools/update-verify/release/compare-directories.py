#! /usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import difflib
import hashlib
import logging
import os
import sys

""" Define the transformations needed to make source + update == target

Required:
The files list describes the files which a transform may be used on.
The 'side' is one of ('source', 'target') and defines where each transform is applied
The 'channel_prefix' list controls which channels a transform may be used for, where a value of
'beta' means all of beta, beta-localtest, beta-cdntest, etc.

One or more:
A 'deletion' specifies a start of line to match on, removing the whole line
A 'substitution' is a list of full string to match and its replacement
"""
TRANSFORMS = [
    # channel-prefs.js
    {
        # preprocessor comments, eg //@line 6 "/builds/worker/workspace/...
        # this can be removed once each channel has a watershed above 59.0b2 (from bug 1431342)
        "files": [
            "defaults/pref/channel-prefs.js",
            "Contents/Resources/defaults/pref/channel-prefs.js",
        ],
        "channel_prefix": ["aurora", "beta", "release", "esr"],
        "side": "source",
        "deletion": '//@line 6 "',
    },
    {
        # updates from a beta to an RC build, the latter specifies the release channel
        "files": [
            "defaults/pref/channel-prefs.js",
            "Contents/Resources/defaults/pref/channel-prefs.js",
        ],
        "channel_prefix": ["beta"],
        "side": "target",
        "substitution": [
            'pref("app.update.channel", "release");\n',
            'pref("app.update.channel", "beta");\n',
        ],
    },
    {
        # updates from an RC to a beta build
        "files": [
            "defaults/pref/channel-prefs.js",
            "Contents/Resources/defaults/pref/channel-prefs.js",
        ],
        "channel_prefix": ["beta"],
        "side": "source",
        "substitution": [
            'pref("app.update.channel", "release");\n',
            'pref("app.update.channel", "beta");\n',
        ],
    },
    {
        # Warning comments from bug 1576546
        # When updating from a pre-70.0 build to 70.0+ this removes the new comments in
        # the target side. In the 70.0+ --> 70.0+ case with a RC we won't need this, and
        # the channel munging above will make channel-prefs.js identical, allowing the code
        # to break before applying this transform.
        "files": [
            "defaults/pref/channel-prefs.js",
            "Contents/Resources/defaults/pref/channel-prefs.js",
        ],
        "channel_prefix": ["aurora", "beta", "release", "esr"],
        "side": "target",
        "deletion": "//",
    },
    # update-settings.ini
    {
        # updates from a beta to an RC build, the latter specifies the release channel
        # on mac, we actually have both files. The second location is the real
        # one but we copy to the first to run the linux64 updater
        "files": ["update-settings.ini", "Contents/Resources/update-settings.ini"],
        "channel_prefix": ["beta"],
        "side": "target",
        "substitution": [
            "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-release\n",
            "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-beta,firefox-mozilla-release\n",
        ],
    },
    {
        # updates from an RC to a beta build
        # on mac, we only need to modify the legit file this time. unpack_build
        # handles the copy for the updater in both source and target
        "files": ["Contents/Resources/update-settings.ini"],
        "channel_prefix": ["beta"],
        "side": "source",
        "substitution": [
            "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-release\n",
            "ACCEPTED_MAR_CHANNEL_IDS=firefox-mozilla-beta,firefox-mozilla-release\n",
        ],
    },
]


def walk_dir(path):
    all_files = []
    all_dirs = []

    for root, dirs, files in os.walk(path):
        all_dirs.extend([os.path.join(root, d) for d in dirs])
        all_files.extend([os.path.join(root, f) for f in files])

    # trim off directory prefix for easier comparison
    all_dirs = [d[len(path) + 1 :] for d in all_dirs]
    all_files = [f[len(path) + 1 :] for f in all_files]

    return all_dirs, all_files


def compare_listings(
    source_list, target_list, label, source_dir, target_dir, ignore_missing=None
):
    obj1 = set(source_list)
    obj2 = set(target_list)
    difference_found = False
    ignore_missing = ignore_missing or ()

    if ignore_missing:
        logging.warning("ignoring paths: {}".format(ignore_missing))

    left_diff = obj1 - obj2
    if left_diff:
        if left_diff - set(ignore_missing):
            _log = logging.error
            difference_found = True
        else:
            _log = logging.warning
            _log("Ignoring missing files due to ignore_missing")

        _log("{} only in {}:".format(label, source_dir))
        for d in sorted(left_diff):
            _log("  {}".format(d))

    right_diff = obj2 - obj1
    if right_diff:
        logging.error("{} only in {}:".format(label, target_dir))
        for d in sorted(right_diff):
            logging.error("  {}".format(d))
        difference_found = True

    return difference_found


def hash_file(filename):
    h = hashlib.sha256()
    with open(filename, "rb", buffering=0) as f:
        for b in iter(lambda: f.read(128 * 1024), b""):
            h.update(b)
    return h.hexdigest()


def compare_common_files(files, channel, source_dir, target_dir):
    difference_found = False
    for filename in files:
        source_file = os.path.join(source_dir, filename)
        target_file = os.path.join(target_dir, filename)

        if os.stat(source_file).st_size != os.stat(target_file).st_size or hash_file(
            source_file
        ) != hash_file(target_file):
            logging.info("Difference found in {}".format(filename))
            file_contents = {
                "source": open(source_file).readlines(),
                "target": open(target_file).readlines(),
            }

            transforms = [
                t
                for t in TRANSFORMS
                if filename in t["files"]
                and channel.startswith(tuple(t["channel_prefix"]))
            ]
            logging.debug(
                "Got {} transform(s) to consider for {}".format(
                    len(transforms), filename
                )
            )
            for transform in transforms:
                side = transform["side"]

                if "deletion" in transform:
                    d = transform["deletion"]
                    logging.debug(
                        "Trying deleting lines starting {} from {}".format(d, side)
                    )
                    file_contents[side] = [
                        l for l in file_contents[side] if not l.startswith(d)
                    ]

                if "substitution" in transform:
                    r = transform["substitution"]
                    logging.debug("Trying replacement for {} in {}".format(r, side))
                    file_contents[side] = [
                        l.replace(r[0], r[1]) for l in file_contents[side]
                    ]

                if file_contents["source"] == file_contents["target"]:
                    logging.info("Transforms removed all differences")
                    break

            if file_contents["source"] != file_contents["target"]:
                difference_found = True
                logging.error(
                    "{} still differs after transforms, residual diff:".format(filename)
                )
                for l in difflib.unified_diff(
                    file_contents["source"], file_contents["target"]
                ):
                    logging.error(l.rstrip())

    return difference_found


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        "Compare two directories recursively, with transformations for expected diffs"
    )
    parser.add_argument("source", help="Directory containing updated Firefox")
    parser.add_argument("target", help="Directory containing expected Firefox")
    parser.add_argument("channel", help="Update channel used")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose logging"
    )
    parser.add_argument(
        "--ignore-missing",
        action="append",
        metavar="<path>",
        help="Ignore absence of <path> in the target",
    )

    args = parser.parse_args()
    level = logging.INFO
    if args.verbose:
        level = logging.DEBUG
    logging.basicConfig(level=level, format="%(message)s", stream=sys.stdout)

    source = args.source
    target = args.target
    if not os.path.exists(source) or not os.path.exists(target):
        logging.error("Source and/or target directory doesn't exist")
        sys.exit(3)

    logging.info("Comparing {} with {}...".format(source, target))
    source_dirs, source_files = walk_dir(source)
    target_dirs, target_files = walk_dir(target)

    dir_list_diff = compare_listings(
        source_dirs, target_dirs, "Directories", source, target
    )
    file_list_diff = compare_listings(
        source_files, target_files, "Files", source, target, args.ignore_missing
    )
    file_diff = compare_common_files(
        set(source_files) & set(target_files), args.channel, source, target
    )

    if file_diff:
        # Use status of 2 since python will use 1 if there is an error running the script
        sys.exit(2)
    elif dir_list_diff or file_list_diff:
        # this has traditionally been a WARN, but we don't have files on one
        # side anymore so lets FAIL
        sys.exit(2)
    else:
        logging.info("No differences found")
