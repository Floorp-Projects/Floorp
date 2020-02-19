# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Script to rename artifacts to a runtime centralized BUILDID-based version
"""

from __future__ import print_function

import argparse
import datetime
import itertools
import os
import re
import sys


ARTIFACT_EXTENSIONS = ('.aar', '.pom', '-sources.jar', '.jar')
HASH_EXTENSIONS = ('', '.sha1', '.md5')
MAVEN_VERSION_REGEX = re.compile(r"(?P<major_number>\d+)\.(?P<minor_number>\d+)\.(?P<patch_number>\d+)", re.VERBOSE)


def _extract_and_check_version(archive_filename, pattern):
    """Function to check if a version-based regex matches a given filename"""
    match = pattern.search(archive_filename)
    try:
        _ = match.group()
    except AttributeError:
        raise Exception(
            'File "{}" present in archive has invalid version. '
            'Expected semver X.Y.Z within in'.format(
                archive_filename
            )
        )

    return match.groupdict()


def _extract_version(files):
    """Function to find and ensure that a single version has been used
    across a component's related files"""
    identifiers = {frozenset(_extract_and_check_version(file_, MAVEN_VERSION_REGEX).items())
        for file_ in files}

    if len(identifiers) > 1:
        # bail if there are different versions across the files
        raise Exception('Different versions identified within the files')
    elif len(identifiers) == 0:
        # bail if no valid version was found
        raise Exception('No valid version was identified within the files')

    return dict(identifiers.pop())


def does_file_name_contain_version(filename):
    """Function to filter out filenames that are part of the release but
    they are not versioned."""

    for extension, hash_extension in itertools.product(ARTIFACT_EXTENSIONS, HASH_EXTENSIONS):
        if filename.endswith(extension + hash_extension):
            return True
    return False


def process_artifacts(path, nightly_version):
    versioned_files = [
        file_name
        for (_, __, file_names) in os.walk(path)
        if file_names
        for file_name in file_names
        if does_file_name_contain_version(file_name)
    ]
    # extract their version and ensure it's unique
    old_version_dict = _extract_version(versioned_files)
    old_version = '{major_number}.{minor_number}.{patch_number}'.format(**old_version_dict)

    # walk recursively again and rename the files accordingly
    for (dirpath, _, filenames) in os.walk(args.path):
        for filename in filenames:
            if old_version in filename:
                new_filename = filename.replace(old_version, nightly_version)
                os.rename(os.path.join(dirpath, filename),
                          os.path.join(dirpath, new_filename))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Rename artifacts to unique runtime BUILDID'
    )
    parser.add_argument(
        '--path',
        help="Dir to specify where the local maven repo is",
        dest='path',
        required=True,
    )
    parser.add_argument(
        '--nightly-version',
        help="Unique decision-task generated nightly-version to be inferred to all artifacts",
        dest='nightly_version',
        required=True,
    )
    args = parser.parse_args()

    if not os.path.isdir(args.path):
        print("Provided path is not a directory")
        sys.exit(2)

    process_artifacts(args.path, args.nightly_version)
