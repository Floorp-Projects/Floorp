# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import os
from six.moves import configparser
import mozpack.path as mozpath


def get_application_ini_value(application_directory, section, value, fallback=None):
    rc = None
    for root, dirs, files in os.walk(application_directory):
        if "application.ini" in files:
            parser = configparser.ConfigParser()
            parser.read(mozpath.join(root, "application.ini"))
            try:
                rc = parser.get(section, value)
            except configparser.NoOptionError:
                if not fallback:
                    raise
                else:
                    rc = parser.get(section, fallback)
            break

    if rc is None:
        raise Exception("Input package does not contain an application.ini file")

    return rc
