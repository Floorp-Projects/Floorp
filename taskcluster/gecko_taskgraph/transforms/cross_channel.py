# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Build a command to run `mach l10n-cross-channel`.
"""


from pipes import quote as shell_quote

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.schema import resolve_keyed_by

transforms = TransformSequence()


@transforms.add
def resolve_keys(config, jobs):
    for job in jobs:
        for item in ["ssh-key-secret", "run.actions"]:
            resolve_keyed_by(job, item, item, **{"level": str(config.params["level"])})
        yield job


@transforms.add
def build_command(config, jobs):
    for job in jobs:
        command = [
            "l10n-cross-channel",
            "-o",
            "/builds/worker/artifacts/outgoing.diff",
            "--attempts",
            "5",
        ]
        ssh_key_secret = job.pop("ssh-key-secret")
        if ssh_key_secret:
            command.extend(["--ssh-secret", ssh_key_secret])
            job.setdefault("scopes", []).append(f"secrets:get:{ssh_key_secret}")

        command.extend(job["run"].pop("actions", []))
        job.setdefault("run", {}).update(
            {"using": "mach", "mach": " ".join(map(shell_quote, command))}
        )
        yield job
