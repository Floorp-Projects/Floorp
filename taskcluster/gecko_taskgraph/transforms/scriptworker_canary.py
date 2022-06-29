# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Build a command to run `mach release push-scriptworker-canaries`.
"""


from pipes import quote as shell_quote

from mozrelease.scriptworker_canary import TASK_TYPES
from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def build_command(config, jobs):
    scriptworkers = config.params["try_task_config"].get(
        "scriptworker-canary-workers", []
    )
    # Filter the list of workers to those we have configured a set of canary
    # tasks for.
    scriptworkers = [
        scriptworker for scriptworker in scriptworkers if scriptworker in TASK_TYPES
    ]

    if not scriptworkers:
        return

    for job in jobs:

        command = ["release", "push-scriptworker-canary"]
        for scriptworker in scriptworkers:
            command.extend(["--scriptworker", scriptworker])
        for address in job.pop("addresses"):
            command.extend(["--address", address])
        if "ssh-key-secret" in job:
            ssh_key_secret = job.pop("ssh-key-secret")
            command.extend(["--ssh-key-secret", ssh_key_secret])
            job.setdefault("scopes", []).append(f"secrets:get:{ssh_key_secret}")

        job.setdefault("run", {}).update(
            {"using": "mach", "mach": " ".join(map(shell_quote, command))}
        )
        yield job
