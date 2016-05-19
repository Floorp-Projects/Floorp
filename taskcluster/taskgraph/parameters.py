# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import sys
import yaml
from mozbuild.util import ReadOnlyDict

class Parameters(ReadOnlyDict):
    """An immutable dictionary with nicer KeyError messages on failure"""
    def __getitem__(self, k):
        try:
            return super(Parameters, self).__getitem__(k)
        except KeyError:
            raise KeyError("taskgraph parameter {!r} not found".format(k))


def load_parameters_file(options):
    """
    Load parameters from the --parameters option
    """
    filename = options['parameters']
    if not filename:
        return Parameters()
    with open(filename) as f:
        if filename.endswith('.yml'):
            return Parameters(**yaml.safe_load(f))
        elif filename.endswith('.json'):
            return Parameters(**json.load(f))
        else:
            raise TypeError("Parameters file `{}` is not JSON or YAML".format(filename))
