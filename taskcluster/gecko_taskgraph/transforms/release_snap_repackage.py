# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.scriptworker import get_release_config
from gecko_taskgraph.util.schema import resolve_keyed_by


transforms = TransformSequence()


@transforms.add
def format(config, tasks):
    """Apply format substitution to worker.env and worker.command."""

    format_params = {
        "release_config": get_release_config(config),
        "config_params": config.params,
    }

    for task in tasks:
        format_params["task"] = task

        command = task.get("worker", {}).get("command", [])
        task["worker"]["command"] = [x.format(**format_params) for x in command]

        env = task.get("worker", {}).get("env", {})
        for k in env.keys():
            resolve_keyed_by(
                env, k, "snap envs", **{"release-level": config.params.release_level()}
            )
            task["worker"]["env"][k] = env[k].format(**format_params)

        yield task
