# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import re
import yaml


def load_yaml(path, name, enforce_order=False):
    """Convenience function to load a YAML file in the given path.  This is
    useful for loading kind configuration files from the kind path.  If
    `enforce_order` is given, then any top-level keys in the file must
    be given in order."""
    filename = os.path.join(path, name)
    if enforce_order:
        keys = []
        key_re = re.compile('^([^ #:]+):')
        with open(filename, "rb") as f:
            for line in f:
                mo = key_re.match(line)
                if mo:
                    keys.append(mo.group(1))
            if keys != list(sorted(keys)):
                raise Exception("keys in {} are not sorted".format(filename))
    with open(filename, "rb") as f:
        return yaml.load(f)
