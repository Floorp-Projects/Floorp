# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the beetmover task into an actual task description.
"""


from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()


@transforms.add
def add_command(config, tasks):
    for task in tasks:
        if not task["worker"].get("env"):
            task["worker"]["env"] = {}

        final_verify_configs = []
        for upstream in sorted(task.get("dependencies", {}).keys()):
            if "update-verify-config" in upstream:
                final_verify_configs.append(
                    f"<{upstream}/public/build/update-verify.cfg>",
                )
        task["run"] = {
            "using": "run-task",
            "cwd": "{checkout}",
            "command": {
                "artifact-reference": "tools/update-verify/release/final-verification.sh "
                + " ".join(final_verify_configs),
            },
            "sparse-profile": "update-verify",
        }
        yield task
