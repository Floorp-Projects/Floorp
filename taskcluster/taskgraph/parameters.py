# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import json
import yaml
from mozbuild.util import ReadOnlyDict

# Please keep this list sorted and in sync with taskcluster/docs/parameters.rst
PARAMETER_NAMES = set([
    'base_repository',
    'build_date',
    'head_ref',
    'head_repository',
    'head_rev',
    'level',
    'message',
    'moz_build_date',
    'optimize_target_tasks',
    'owner',
    'project',
    'pushdate',
    'pushlog_id',
    'target_tasks_method',
    'triggered_by',
])


class Parameters(ReadOnlyDict):
    """An immutable dictionary with nicer KeyError messages on failure"""
    def check(self):
        names = set(self)
        msg = []

        missing = PARAMETER_NAMES - names
        if missing:
            msg.append("missing parameters: " + ", ".join(missing))

        extra = names - PARAMETER_NAMES
        if extra:
            msg.append("extra parameters: " + ", ".join(extra))

        if msg:
            raise Exception("; ".join(msg))

    def __getitem__(self, k):
        if k not in PARAMETER_NAMES:
            raise KeyError("no such parameter {!r}".format(k))
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
