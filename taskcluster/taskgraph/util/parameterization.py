# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import re

from taskgraph.util.time import json_time_from_now

TASK_REFERENCE_PATTERN = re.compile('<([^>]+)>')


def _recurse(val, param_name, param_fn):
    param_keys = [param_name]

    def recurse(val):
        if isinstance(val, list):
            return [recurse(v) for v in val]
        elif isinstance(val, dict):
            if val.keys() == param_keys:
                return param_fn(val[param_name])
            else:
                return {k: recurse(v) for k, v in val.iteritems()}
        else:
            return val
    return recurse(val)


def resolve_timestamps(now, task_def):
    """Resolve all instances of `{'relative-datestamp': '..'}` in the given task definition"""
    return _recurse(task_def, 'relative-datestamp', lambda v: json_time_from_now(v, now))


def resolve_task_references(label, task_def, dependencies):
    """Resolve all instances of `{'task-reference': '..<..>..'}` in the given task
    definition, using the given dependencies"""
    def repl(match):
        key = match.group(1)
        try:
            return dependencies[key]
        except KeyError:
            # handle escaping '<'
            if key == '<':
                return key
            raise KeyError("task '{}' has no dependency named '{}'".format(label, key))

    return _recurse(task_def, 'task-reference', lambda v: TASK_REFERENCE_PATTERN.sub(repl, v))
