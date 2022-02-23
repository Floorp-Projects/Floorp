# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re

from taskgraph.util.time import json_time_from_now
from taskgraph.util.taskcluster import get_artifact_url

TASK_REFERENCE_PATTERN = re.compile("<([^>]+)>")
ARTIFACT_REFERENCE_PATTERN = re.compile("<([^/]+)/([^>]+)>")


def _recurse(val, param_fns):
    def recurse(val):
        if isinstance(val, list):
            return [recurse(v) for v in val]
        elif isinstance(val, dict):
            if len(val) == 1:
                for param_key, param_fn in param_fns.items():
                    if set(val.keys()) == {param_key}:
                        return param_fn(val[param_key])
            return {k: recurse(v) for k, v in val.items()}
        else:
            return val

    return recurse(val)


def resolve_timestamps(now, task_def):
    """Resolve all instances of `{'relative-datestamp': '..'}` in the given task definition"""
    return _recurse(
        task_def,
        {
            "relative-datestamp": lambda v: json_time_from_now(v, now),
        },
    )


def resolve_task_references(label, task_def, task_id, decision_task_id, dependencies):
    """Resolve all instances of ``{'task-reference': '..<..>..'} ``
    and ``{'artifact-reference`: '..<dependency/artifact/path>..'}``
    in the given task definition, using the given dependencies.
    """

    def task_reference(val):
        def repl(match):
            key = match.group(1)
            if key == "self":
                return task_id
            elif key == "decision":
                return decision_task_id
            try:
                return dependencies[key]
            except KeyError:
                # handle escaping '<'
                if key == "<":
                    return key
                raise KeyError(f"task '{label}' has no dependency named '{key}'")

        return TASK_REFERENCE_PATTERN.sub(repl, val)

    def artifact_reference(val):
        def repl(match):
            dependency, artifact_name = match.group(1, 2)

            if dependency == "self":
                raise KeyError(f"task '{label}' can't reference artifacts of self")
            elif dependency == "decision":
                task_id = decision_task_id
            else:
                try:
                    task_id = dependencies[dependency]
                except KeyError:
                    raise KeyError(
                        "task '{}' has no dependency named '{}'".format(
                            label, dependency
                        )
                    )

            assert artifact_name.startswith(
                "public/"
            ), "artifact-reference only supports public artifacts, not `{}`".format(
                artifact_name
            )
            return get_artifact_url(task_id, artifact_name)

        return ARTIFACT_REFERENCE_PATTERN.sub(repl, val)

    return _recurse(
        task_def,
        {
            "task-reference": task_reference,
            "artifact-reference": artifact_reference,
        },
    )
