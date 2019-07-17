"""
Script to rename snapshots artifacts to a runtime centralized TIMESTAMP
"""

from __future__ import print_function

import argparse
import datetime
import itertools
import os
import re
import sys

from decision_task import AAR_EXTENSIONS, HASH_EXTENSIONS


SNAPSHOT_TIMESTAMP_REGEX = r'\d{8}\.\d{6}-\d{1}'


def _extract_and_check_timestamps(archive_filename, regex):
    """Function to check if a timestamp-based regex matches a given filename"""
    match = re.search(regex, archive_filename)
    try:
        identifier = match.group()
    except AttributeError:
        raise Exception(
            'File "{}" present in archive has invalid identifier. '
            'Expected YYYYMMDD.HHMMSS-BUILDNUMBER within in'.format(
                archive_filename
            )
        )
    timestamp, buildno = identifier.split('-')
    try:
        datetime.datetime.strptime(timestamp, '%Y%m%d.%H%M%S')
    except ValueError:
        raise Exception(
            'File "{}" present in archive has invalid timestamp. '
            'Expected YYYYMMDD.HHMMSS within in'.format(
                archive_filename
            )
        )

    date, clock = timestamp.split('.')
    return date, clock, buildno


def _extract_old_timestamp(files):
    """Function to find and ensure that a single timestamp has been used
    across a component's related files"""
    identifiers_collection = set()
    for file_ in files:
        (date, clock, bno) = _extract_and_check_timestamps(file_,
                                                           SNAPSHOT_TIMESTAMP_REGEX)
        _args = {
            'date': date,
            'clock': clock,
            'buildno': bno
        }
        identifiers_collection.add(frozenset(_args.items()))

    if len(identifiers_collection) > 1:
        # bail if there are different timestamps/buildnumbers across the files
        raise Exception('Different buildnumbers/timestamps identified within the files')
    elif len(identifiers_collection) == 0:
        # bail if no valid timestamp was found
        raise Exception('No valid timestamp was identified within the files')

    old_timestamp_dict = dict(next(iter(identifiers_collection)))
    return '{date}.{clock}-{buildno}'.format(**old_timestamp_dict)


def is_snapshot_timestamped_file(filename):
    """Function to filter out filenames that are part of the snapshot releae but
    they are not timestamped."""

    for extension, hash_extension in itertools.product(AAR_EXTENSIONS, HASH_EXTENSIONS):
        if filename.endswith(extension + hash_extension):
            return True
    return False


def process_snapshots_artifacts(path, timestamp):
    files = []
    # gather all the files recursively
    for (dirpath, dirnames, filenames) in os.walk(args.path):
        if filenames:
            files.extend(filenames)

    # exclude files that are not timestamped such as metadata.xml
    files = filter(lambda f: is_snapshot_timestamped_file(f), files)

    # extract their timestamps
    old_timestamp = _extract_old_timestamp(files)

    # walk recursively again and rename the files accordingly
    for (dirpath, dirnames, filenames) in os.walk(args.path):
        for filename in filenames:
            if old_timestamp in filename:
                new_filename = filename.replace(old_timestamp, timestamp)
                os.rename(os.path.join(dirpath, filename),
                        os.path.join(dirpath, new_filename))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='Rename SNAPSHOT artifacts to unieque runtime TIMESTAMP'
    )
    parser.add_argument(
        '--path',
        help="Dir to specify where the local maven repo is",
        dest='path',
        required=True,
    )
    parser.add_argument(
        '--timestamp',
        help="Unique decision-task generated timestamp to be inferred to all artifacts",
        dest='timestamp',
        required=True,
    )
    args = parser.parse_args()

    if not os.path.isdir(args.path):
        print("Provided path is not a directory")
        sys.exit(2)

    process_snapshots_artifacts(args.path, args.timestamp)
