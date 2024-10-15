# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


from shlex import quote as shell_quote

from gecko_taskgraph.transforms.job import configure_taskdesc_for_run, run_job_using
from taskgraph.util import path
from taskgraph.util.schema import Schema, taskref_or_string
from voluptuous import Optional, Required

secret_schema = {
    Required("name"): str,
    Required("path"): str,
    Required("key"): str,
    Optional("json"): bool,
    Optional("decode"): bool,
}

dummy_secret_schema = {
    Required("content"): str,
    Required("path"): str,
    Optional("json"): bool,
}

gradlew_schema = Schema(
    {
        Required("using"): "gradlew",
        Optional("pre-gradlew"): [[str]],
        Required("gradlew"): [str],
        Optional("post-gradlew"): [[str]],
        # Base work directory used to set up the task.
        Required("workdir"): str,
        Optional("use-caches"): bool,
        Optional("secrets"): [secret_schema],
        Optional("dummy-secrets"): [dummy_secret_schema],
    }
)

run_commands_schema = Schema(
    {
        Required("using"): "run-commands",
        Optional("pre-commands"): [[str]],
        Required("commands"): [[taskref_or_string]],
        Required("workdir"): str,
        Optional("use-caches"): bool,
        Optional("secrets"): [secret_schema],
        Optional("dummy-secrets"): [dummy_secret_schema],
    }
)


@run_job_using("docker-worker", "run-commands", schema=run_commands_schema)
def configure_run_commands_schema(config, job, taskdesc):
    run = job["run"]
    pre_commands = run.pop("pre-commands", [])
    pre_commands += [
        _generate_dummy_secret_command(secret)
        for secret in run.pop("dummy-secrets", [])
    ]
    pre_commands += [
        _generate_secret_command(secret) for secret in run.get("secrets", [])
    ]

    all_commands = pre_commands + run.pop("commands", [])

    run["command"] = _convert_commands_to_string(all_commands)
    _inject_secrets_scopes(run, taskdesc)
    _set_run_task_attributes(job)
    configure_taskdesc_for_run(config, job, taskdesc, job["worker"]["implementation"])


@run_job_using("docker-worker", "gradlew", schema=gradlew_schema)
def configure_gradlew(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]

    fetches_dir = "/builds/worker/fetches"
    topsrc_dir = "/builds/worker/checkouts/gecko"
    worker.setdefault("env", {}).update(
        {
            "ANDROID_SDK_ROOT": path.join(fetches_dir, "android-sdk-linux"),
            "GRADLE_USER_HOME": path.join(
                topsrc_dir, "mobile/android/gradle/dotgradle-online"
            ),
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
        }
    )
    worker["env"].setdefault(
        "MOZCONFIG",
        path.join(
            topsrc_dir,
            "mobile/android/config/mozconfigs/android-arm/nightly-android-lints",
        ),
    )
    worker["env"].setdefault(
        "MOZ_ANDROID_FAT_AAR_ARCHITECTURES", "armeabi-v7a,arm64-v8a,x86,x86_64"
    )

    dummy_secrets = [
        _generate_dummy_secret_command(secret)
        for secret in run.pop("dummy-secrets", [])
    ]
    secrets = [_generate_secret_command(secret) for secret in run.get("secrets", [])]
    worker["env"].update(
        {
            "PRE_GRADLEW": _convert_commands_to_string(run.pop("pre-gradlew", [])),
            "GET_SECRETS": _convert_commands_to_string(dummy_secrets + secrets),
            "GRADLEW_ARGS": " ".join(run.pop("gradlew")),
            "POST_GRADLEW": _convert_commands_to_string(run.pop("post-gradlew", [])),
        }
    )
    run[
        "command"
    ] = "/builds/worker/checkouts/gecko/taskcluster/scripts/builder/build-android.sh"
    _inject_secrets_scopes(run, taskdesc)
    _set_run_task_attributes(job)
    configure_taskdesc_for_run(config, job, taskdesc, job["worker"]["implementation"])


def _generate_secret_command(secret):
    secret_command = [
        "/builds/worker/checkouts/gecko/taskcluster/scripts/get-secret.py",
        "-s",
        secret["name"],
        "-k",
        secret["key"],
        "-f",
        secret["path"],
    ]
    if secret.get("json"):
        secret_command.append("--json")

    if secret.get("decode"):
        secret_command.append("--decode")

    return secret_command


def _generate_dummy_secret_command(secret):
    secret_command = [
        "/builds/worker/checkouts/gecko/taskcluster/scripts/write-dummy-secret.py",
        "-f",
        secret["path"],
        "-c",
        secret["content"],
    ]
    if secret.get("json"):
        secret_command.append("--json")

    return secret_command


def _convert_commands_to_string(commands):
    should_artifact_reference = False
    should_task_reference = False

    sanitized_commands = []
    for command in commands:
        sanitized_parts = []
        for part in command:
            if isinstance(part, dict):
                if "artifact-reference" in part:
                    part_string = part["artifact-reference"]
                    should_artifact_reference = True
                elif "task-reference" in part:
                    part_string = part["task-reference"]
                    should_task_reference = True
                else:
                    raise ValueError(f"Unsupported dict: {part}")
            else:
                part_string = part

            sanitized_parts.append(part_string)
        sanitized_commands.append(sanitized_parts)

    shell_quoted_commands = [
        " ".join(map(shell_quote, command)) for command in sanitized_commands
    ]
    full_string_command = " && ".join(shell_quoted_commands)

    if should_artifact_reference and should_task_reference:
        raise NotImplementedError(
            '"arifact-reference" and "task-reference" cannot be both used'
        )
    elif should_artifact_reference:
        return {"artifact-reference": full_string_command}
    elif should_task_reference:
        return {"task-reference": full_string_command}
    else:
        return full_string_command


def _inject_secrets_scopes(run, taskdesc):
    secrets = run.pop("secrets", [])
    scopes = taskdesc.setdefault("scopes", [])
    new_secret_scopes = ["secrets:get:{}".format(secret["name"]) for secret in secrets]
    new_secret_scopes = list(
        set(new_secret_scopes)
    )  # Scopes must not have any duplicates
    scopes.extend(new_secret_scopes)


def _set_run_task_attributes(job):
    run = job["run"]
    run["cwd"] = "{checkout}"
    run["using"] = "run-task"
