# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import

import yaml
from perfdocs.logger import PerfDocLogger

logger = PerfDocLogger()


def read_file(path):
    '''Opens a file and returns its contents.

    :param str path: Path to the file.
    :return list: List containing the lines in the file.
    '''
    with open(path, 'r') as f:
        return f.readlines()


def read_yaml(yaml_path):
    '''
    Opens a YAML file and returns the contents.

    :param str yaml_path: Path to the YAML to open.
    :return dict: Dictionary containing the YAML content.
    '''
    contents = {}
    try:
        with open(yaml_path, 'r') as f:
            contents = yaml.safe_load(f)
    except Exception as e:
        logger.warning(
            "Error opening file {}: {}".format(yaml_path, str(e)), yaml_path
        )

    return contents
