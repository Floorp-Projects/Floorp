# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import difflib
import filecmp
import os
import yaml
from perfdocs.logger import PerfDocLogger

logger = PerfDocLogger()

ON_TRY = "MOZ_AUTOMATION" in os.environ


def save_file(file_content, path, extension="rst"):
    """
    Saves data into a file.

    :param str path: Location and name of the file being saved
        (without an extension).
    :param str data: Content to write into the file.
    :param str extension: Extension to save the file as.
    """
    with open("{}.{}".format(path, extension), "w", newline="\n") as f:
        f.write(file_content)


def read_file(path, stringify=False):
    """
    Opens a file and returns its contents.

    :param str path: Path to the file.
    :return list: List containing the lines in the file.
    """
    with open(path, "r") as f:
        return f.read() if stringify else f.readlines()


def read_yaml(yaml_path):
    """
    Opens a YAML file and returns the contents.

    :param str yaml_path: Path to the YAML to open.
    :return dict: Dictionary containing the YAML content.
    """
    contents = {}
    try:
        with open(yaml_path, "r") as f:
            contents = yaml.safe_load(f)
    except Exception as e:
        logger.warning("Error opening file {}: {}".format(yaml_path, str(e)), yaml_path)

    return contents


def are_dirs_equal(dir_1, dir_2):
    """
    Compare two directories to see if they are equal. Files in each
    directory are assumed to be equal if their names and contents
    are equal.

    :param dir_1: First directory path
    :param dir_2: Second directory path
    :return: True if the directory trees are the same and False otherwise.
    """

    dirs_cmp = filecmp.dircmp(dir_1, dir_2)
    if dirs_cmp.left_only or dirs_cmp.right_only or dirs_cmp.funny_files:
        logger.log("Some files are missing or are funny.")
        for file in dirs_cmp.left_only:
            logger.log(f"Missing in existing docs: {file}")
        for file in dirs_cmp.right_only:
            logger.log(f"Missing in new docs: {file}")
        for file in dirs_cmp.funny_files:
            logger.log(f"The following file is funny: {file}")
        return False

    _, mismatch, errors = filecmp.cmpfiles(
        dir_1, dir_2, dirs_cmp.common_files, shallow=False
    )

    if mismatch or errors:
        logger.log(f"Found mismatches: {mismatch}")

        # The root for where to save the diff will be different based on
        # whether we are running in CI or not
        diff_filename = "diff.txt"
        diff_root = "/builds/worker/"
        if not ON_TRY:
            diff_root = f"{PerfDocLogger.TOP_DIR}artifacts/"
        diff_path = f"{diff_root}{diff_filename}"

        with open(diff_path, "w") as diff_file:
            for entry in mismatch:
                logger.log(f"Mismatch found on {entry}")

                with open(os.path.join(dir_1, entry)) as f:
                    newlines = f.readlines()
                with open(os.path.join(dir_2, entry)) as f:
                    baselines = f.readlines()
                for line in difflib.unified_diff(
                    baselines, newlines, fromfile="base", tofile="new"
                ):
                    logger.log(line)

                # Here we want to add to diff.txt in a patch format, we use
                # the basedir to make the file names/paths relative and this is
                # different in CI vs local runs.
                basedir = "/builds/worker/checkouts/gecko/"
                if not ON_TRY:
                    basedir = diff_root

                relative_path = os.path.join(dir_2, entry).split(basedir)[-1]
                patch = difflib.unified_diff(
                    baselines, newlines, fromfile=relative_path, tofile=relative_path
                )

                write_header = True
                for line in patch:
                    if write_header:
                        diff_file.write(
                            f"diff --git a/{relative_path} b/{relative_path}\n"
                        )
                        write_header = False
                    diff_file.write(line)

                logger.log(f"Completed diff on {entry}")

        logger.log(f"Saved diff to {diff_path}")

        return False

    for common_dir in dirs_cmp.common_dirs:
        subdir_1 = os.path.join(dir_1, common_dir)
        subdir_2 = os.path.join(dir_2, common_dir)
        if not are_dirs_equal(subdir_1, subdir_2):
            return False

    return True
