# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.job import run_job_using, configure_taskdesc_for_run
from taskgraph.util import path
from taskgraph.util.schema import Schema
from voluptuous import Required, Optional
from six import text_type

from pipes import quote as shell_quote

gradlew_schema = Schema(
    {
        Required("using"): "gradlew",
        Optional("pre-gradlew"): [[text_type]],
        Required("gradlew"): [text_type],
        Optional("post-gradlew"): [[text_type]],
        # Base work directory used to set up the task.
        Required("workdir"): text_type,
        Optional("use-caches"): bool,
        Optional("secrets"): [
            {
                Required("name"): text_type,
                Required("path"): text_type,
                Required("key"): text_type,
                Optional("json"): bool,
            }
        ],
    }
)


@run_job_using("docker-worker", "gradlew", schema=gradlew_schema)
def configure_gradlew(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]

    worker.setdefault("env", {}).update(
        {"ANDROID_SDK_ROOT": path.join(run["workdir"], "android-sdk-linux")}
    )

    # defer to the run_task implementation
    run["command"] = _extract_command(run)
    secrets = run.pop("secrets", [])
    scopes = taskdesc.setdefault("scopes", [])
    new_secret_scopes = ["secrets:get:{}".format(secret["name"]) for secret in secrets]
    new_secret_scopes = list(set(new_secret_scopes))  # Scopes must not have any duplicates
    scopes.extend(new_secret_scopes)

    run["cwd"] = "{checkout}"
    run["using"] = "run-task"
    configure_taskdesc_for_run(config, job, taskdesc, job["worker"]["implementation"])


def _extract_command(run):
    pre_gradle_commands = run.pop("pre-gradlew", [])
    pre_gradle_commands += [
        _generate_secret_command(secret) for secret in run.get("secrets", [])
    ]

    gradle_command = ["./gradlew"] + run.pop("gradlew")
    post_gradle_commands = run.pop("post-gradlew", [])

    commands = pre_gradle_commands + [gradle_command] + post_gradle_commands
    shell_quoted_commands = [" ".join(map(shell_quote, command)) for command in commands]
    return " && ".join(shell_quoted_commands)


def _generate_secret_command(secret):
    secret_command = [
        "automation/taskcluster/helper/get-secret.py",
        "-s", secret["name"],
        "-k", secret["key"],
        "-f", secret["path"],
    ]
    if secret.get("json"):
        secret_command.append("--json")

    return secret_command
