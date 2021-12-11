# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import filecmp
import os
import yaml
from perfdocs.logger import PerfDocLogger

logger = PerfDocLogger()


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
        return False

    _, mismatch, errors = filecmp.cmpfiles(
        dir_1, dir_2, dirs_cmp.common_files, shallow=False
    )

    if mismatch or errors:
        return False

    for common_dir in dirs_cmp.common_dirs:
        subdir_1 = os.path.join(dir_1, common_dir)
        subdir_2 = os.path.join(dir_2, common_dir)
        if not are_dirs_equal(subdir_1, subdir_2):
            return False

    return True
