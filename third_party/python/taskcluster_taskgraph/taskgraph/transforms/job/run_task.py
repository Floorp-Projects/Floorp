# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""


import os

import attr
from voluptuous import Any, Optional, Required

from taskgraph.transforms.job import run_job_using
from taskgraph.transforms.job.common import support_vcs_checkout
from taskgraph.transforms.task import taskref_or_string
from taskgraph.util import path, taskcluster
from taskgraph.util.schema import Schema

EXEC_COMMANDS = {
    "bash": ["bash", "-cx"],
    "powershell": ["powershell.exe", "-ExecutionPolicy", "Bypass"],
}

run_task_schema = Schema(
    {
        Required("using"): "run-task",
        # if true, add a cache at ~worker/.cache, which is where things like pip
        # tend to hide their caches.  This cache is never added for level-1 jobs.
        # TODO Once bug 1526028 is fixed, this and 'use-caches' should be merged.
        Required("cache-dotcache"): bool,
        # Whether or not to use caches.
        Optional("use-caches"): bool,
        # if true (the default), perform a checkout on the worker
        Required("checkout"): Any(bool, {str: dict}),
        Optional(
            "cwd",
            description="Path to run command in. If a checkout is present, the path "
            "to the checkout will be interpolated with the key `checkout`",
        ): str,
        # The sparse checkout profile to use. Value is the filename relative to the
        # directory where sparse profiles are defined (build/sparse-profiles/).
        Required("sparse-profile"): Any(str, None),
        # The command arguments to pass to the `run-task` script, after the
        # checkout arguments.  If a list, it will be passed directly; otherwise
        # it will be included in a single argument to the command specified by
        # `exec-with`.
        Required("command"): Any([taskref_or_string], taskref_or_string),
        # Context to substitute into the command using format string
        # substitution (e.g {value}). This is useful if certain aspects of the
        # command need to be generated in transforms.
        Optional("command-context"): dict,
        # What to execute the command with in the event command is a string.
        Optional("exec-with"): Any(*list(EXEC_COMMANDS)),
        # Base work directory used to set up the task.
        Required("workdir"): str,
        # Whether to run as root. (defaults to False)
        Optional("run-as-root"): bool,
    }
)


def common_setup(config, job, taskdesc, command):
    run = job["run"]
    if run["checkout"]:
        repo_configs = config.repo_configs
        if len(repo_configs) > 1 and run["checkout"] is True:
            raise Exception("Must explicitly specify checkouts with multiple repos.")
        elif run["checkout"] is not True:
            repo_configs = {
                repo: attr.evolve(repo_configs[repo], **config)
                for (repo, config) in run["checkout"].items()
            }

        support_vcs_checkout(
            config,
            job,
            taskdesc,
            repo_configs=repo_configs,
            sparse=bool(run["sparse-profile"]),
        )

        vcs_path = taskdesc["worker"]["env"]["VCS_PATH"]
        for repo_config in repo_configs.values():
            checkout_path = path.join(vcs_path, repo_config.path)
            command.append(f"--{repo_config.prefix}-checkout={checkout_path}")

        if run["sparse-profile"]:
            command.append(
                "--{}-sparse-profile=build/sparse-profiles/{}".format(
                    repo_config.prefix,
                    run["sparse-profile"],
                )
            )

        if "cwd" in run:
            run["cwd"] = path.normpath(run["cwd"].format(checkout=vcs_path))
    elif "cwd" in run and "{checkout}" in run["cwd"]:
        raise Exception(
            "Found `{{checkout}}` interpolation in `cwd` for task {name} "
            "but the task doesn't have a checkout: {cwd}".format(
                cwd=run["cwd"], name=job.get("name", job.get("label"))
            )
        )

    if "cwd" in run:
        command.extend(("--task-cwd", run["cwd"]))

    taskdesc["worker"].setdefault("env", {})["MOZ_SCM_LEVEL"] = config.params["level"]


worker_defaults = {
    "cache-dotcache": False,
    "checkout": True,
    "sparse-profile": None,
    "run-as-root": False,
}


def script_url(config, script):
    if "MOZ_AUTOMATION" in os.environ and "TASK_ID" not in os.environ:
        raise Exception("TASK_ID must be defined to use run-task on generic-worker")
    task_id = os.environ.get("TASK_ID", "<TASK_ID>")
    # use_proxy = False to avoid having all generic-workers turn on proxy
    # Assumes the cluster allows anonymous downloads of public artifacts
    tc_url = taskcluster.get_root_url(False)
    # TODO: Use util/taskcluster.py:get_artifact_url once hack for Bug 1405889 is removed
    return f"{tc_url}/api/queue/v1/task/{task_id}/artifacts/public/{script}"


@run_job_using(
    "docker-worker", "run-task", schema=run_task_schema, defaults=worker_defaults
)
def docker_worker_run_task(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]
    command = ["/usr/local/bin/run-task"]
    common_setup(config, job, taskdesc, command)

    if run.get("cache-dotcache"):
        worker["caches"].append(
            {
                "type": "persistent",
                "name": "{project}-dotcache".format(**config.params),
                "mount-point": "{workdir}/.cache".format(**run),
                "skip-untrusted": True,
            }
        )

    run_command = run["command"]

    command_context = run.get("command-context")
    if command_context:
        run_command = run_command.format(**command_context)

    # dict is for the case of `{'task-reference': str}`.
    if isinstance(run_command, str) or isinstance(run_command, dict):
        exec_cmd = EXEC_COMMANDS[run.pop("exec-with", "bash")]
        run_command = exec_cmd + [run_command]
    if run["run-as-root"]:
        command.extend(("--user", "root", "--group", "root"))
    command.append("--")
    command.extend(run_command)
    worker["command"] = command


@run_job_using(
    "generic-worker", "run-task", schema=run_task_schema, defaults=worker_defaults
)
def generic_worker_run_task(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]
    is_win = worker["os"] == "windows"
    is_mac = worker["os"] == "macosx"
    is_bitbar = worker["os"] == "linux-bitbar"

    if is_win:
        command = ["C:/mozilla-build/python3/python3.exe", "run-task"]
    elif is_mac:
        command = ["/tools/python36/bin/python3", "run-task"]
    else:
        command = ["./run-task"]

    common_setup(config, job, taskdesc, command)

    worker.setdefault("mounts", [])
    if run.get("cache-dotcache"):
        worker["mounts"].append(
            {
                "cache-name": "{project}-dotcache".format(**config.params),
                "directory": "{workdir}/.cache".format(**run),
            }
        )
    worker["mounts"].append(
        {
            "content": {
                "url": script_url(config, "run-task"),
            },
            "file": "./run-task",
        }
    )
    if worker.get("env", {}).get("MOZ_FETCHES"):
        worker["mounts"].append(
            {
                "content": {
                    "url": script_url(config, "fetch-content"),
                },
                "file": "./fetch-content",
            }
        )

    run_command = run["command"]

    if isinstance(run_command, str):
        if is_win:
            run_command = f'"{run_command}"'
        exec_cmd = EXEC_COMMANDS[run.pop("exec-with", "bash")]
        run_command = exec_cmd + [run_command]

    command_context = run.get("command-context")
    if command_context:
        for i in range(len(run_command)):
            run_command[i] = run_command[i].format(**command_context)

    if run["run-as-root"]:
        command.extend(("--user", "root", "--group", "root"))
    command.append("--")
    if is_bitbar:
        # Use the bitbar wrapper script which sets up the device and adb
        # environment variables
        command.append("/builds/taskcluster/script.py")
    command.extend(run_command)

    if is_win:
        worker["command"] = [" ".join(command)]
    else:
        worker["command"] = [
            ["chmod", "+x", "run-task"],
            command,
        ]
