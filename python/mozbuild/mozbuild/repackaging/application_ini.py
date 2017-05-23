# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import ConfigParser
import mozpack.path as mozpath

def get_application_ini_value(application_directory, section, value):
    rc = None
    for root, dirs, files in os.walk(application_directory):
        if 'application.ini' in files:
            parser = ConfigParser.ConfigParser()
            parser.read(mozpath.join(root, 'application.ini'))
            rc = parser.get(section, value)
            break

    if rc is None:
        raise Exception("Input package does not contain an application.ini file")

    return rc
