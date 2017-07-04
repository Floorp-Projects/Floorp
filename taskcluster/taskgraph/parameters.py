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
    'filters',
    'head_ref',
    'head_repository',
    'head_rev',
    'include_nightly',
    'level',
    'message',
    'moz_build_date',
    'optimize_target_tasks',
    'owner',
    'project',
    'pushdate',
    'pushlog_id',
    'target_tasks_method',
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


def load_parameters_file(filename):
    """
    Load parameters from a path, url, decision task-id or project.

    Examples:
        task-id=fdtgsD5DQUmAQZEaGMvQ4Q
        project=mozilla-central
    """
    import urllib
    from taskgraph.util.taskcluster import get_artifact_url, find_task_id

    if not filename:
        return Parameters()

    try:
        # reading parameters from a local parameters.yml file
        f = open(filename)
    except IOError:
        # fetching parameters.yml using task task-id, project or supplied url
        task_id = None
        if filename.startswith("task-id="):
            task_id = filename.split("=")[1]
        elif filename.startswith("project="):
            index = "gecko.v2.{}.latest.firefox.decision".format(filename.split("=")[1])
            task_id = find_task_id(index)

        if task_id:
            filename = get_artifact_url(task_id, 'public/parameters.yml')
        f = urllib.urlopen(filename)

    if filename.endswith('.yml'):
        return Parameters(**yaml.safe_load(f))
    elif filename.endswith('.json'):
        return Parameters(**json.load(f))
    else:
        raise TypeError("Parameters file `{}` is not JSON or YAML".format(filename))
