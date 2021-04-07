# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running jobs that are invoked via the `run-task` script.
"""

from __future__ import absolute_import, print_function, unicode_literals

from six import text_type

from mozpack import path

from taskgraph.transforms.task import taskref_or_string
from taskgraph.transforms.job import run_job_using
from taskgraph.util.schema import Schema
from taskgraph.transforms.job.common import add_tooltool, support_vcs_checkout
from voluptuous import Any, Optional, Required

run_task_schema = Schema(
    {
        Required("using"): "run-task",
        # if true, add a cache at ~worker/.cache, which is where things like pip
        # tend to hide their caches.  This cache is never added for level-1 jobs.
        # TODO Once bug 1526028 is fixed, this and 'use-caches' should be merged.
        Required("cache-dotcache"): bool,
        # Whether or not to use caches.
        Optional("use-caches"): bool,
        # if true (the default), perform a checkout of gecko on the worker
        Required("checkout"): bool,
        Optional(
            "cwd",
            description="Path to run command in. If a checkout is present, the path "
            "to the checkout will be interpolated with the key `checkout`",
        ): text_type,
        # The sparse checkout profile to use. Value is the filename relative to
        # "sparse-profile-prefix" which defaults to "build/sparse-profiles/".
        Required("sparse-profile"): Any(text_type, None),
        # The relative path to the sparse profile.
        Optional("sparse-profile-prefix"): text_type,
        # if true, perform a checkout of a comm-central based branch inside the
        # gecko checkout
        Required("comm-checkout"): bool,
        # The command arguments to pass to the `run-task` script, after the
        # checkout arguments.  If a list, it will be passed directly; otherwise
        # it will be included in a single argument to `bash -cx`.
        Required("command"): Any([taskref_or_string], taskref_or_string),
        # Base work directory used to set up the task.
        Optional("workdir"): text_type,
        # If not false, tooltool downloads will be enabled via relengAPIProxy
        # for either just public files, or all files. Only supported on
        # docker-worker.
        Required("tooltool-downloads"): Any(
            False,
            "public",
            "internal",
        ),
        # Whether to run as root. (defaults to False)
        Optional("run-as-root"): bool,
    }
)


def common_setup(config, job, taskdesc, command):
    run = job["run"]
    if run["checkout"]:
        support_vcs_checkout(config, job, taskdesc, sparse=bool(run["sparse-profile"]))
        command.append(
            "--gecko-checkout={}".format(taskdesc["worker"]["env"]["GECKO_PATH"])
        )

    if run["sparse-profile"]:
        sparse_profile_prefix = run.pop(
            "sparse-profile-prefix", "build/sparse-profiles"
        )
        sparse_profile_path = path.join(sparse_profile_prefix, run["sparse-profile"])
        command.append("--gecko-sparse-profile={}".format(sparse_profile_path))

    taskdesc["worker"].setdefault("env", {})["MOZ_SCM_LEVEL"] = config.params["level"]


worker_defaults = {
    "cache-dotcache": False,
    "checkout": True,
    "comm-checkout": False,
    "sparse-profile": None,
    "tooltool-downloads": False,
    "run-as-root": False,
}


def script_url(config, script):
    return config.params.file_url(
        "taskcluster/scripts/{}".format(script),
    )


@run_job_using(
    "docker-worker", "run-task", schema=run_task_schema, defaults=worker_defaults
)
def docker_worker_run_task(config, job, taskdesc):
    run = job["run"]
    worker = taskdesc["worker"] = job["worker"]
    command = ["/builds/worker/bin/run-task"]
    common_setup(config, job, taskdesc, command)

    if run["tooltool-downloads"]:
        internal = run["tooltool-downloads"] == "internal"
        add_tooltool(config, job, taskdesc, internal=internal)

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
    run_cwd = run.get("cwd")
    if run_cwd and run["checkout"]:
        run_cwd = path.normpath(
            run_cwd.format(checkout=taskdesc["worker"]["env"]["GECKO_PATH"])
        )
    elif run_cwd and "{checkout}" in run_cwd:
        raise Exception(
            "Found `{{checkout}}` interpolation in `cwd` for task {name} "
            "but the task doesn't have a checkout: {cwd}".format(
                cwd=run_cwd, name=job.get("name", job.get("label"))
            )
        )

    # dict is for the case of `{'task-reference': text_type}`.
    if isinstance(run_command, (text_type, dict)):
        run_command = ["bash", "-cx", run_command]
    if run["comm-checkout"]:
        command.append(
            "--comm-checkout={}/comm".format(taskdesc["worker"]["env"]["GECKO_PATH"])
        )
    command.append("--fetch-hgfingerprint")
    if run["run-as-root"]:
        command.extend(("--user", "root", "--group", "root"))
    if run_cwd:
        command.extend(("--task-cwd", run_cwd))
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

    if run["tooltool-downloads"]:
        internal = run["tooltool-downloads"] == "internal"
        add_tooltool(config, job, taskdesc, internal=internal)

    if is_win:
        command = ["C:/mozilla-build/python3/python3.exe", "run-task"]
    elif is_mac:
        command = ["/usr/local/bin/python3", "run-task"]
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
                    "url": script_url(config, "misc/fetch-content"),
                },
                "file": "./fetch-content",
            }
        )

    run_command = run["command"]
    run_cwd = run.get("cwd")
    if run_cwd and run["checkout"]:
        run_cwd = path.normpath(
            run_cwd.format(checkout=taskdesc["worker"]["env"]["GECKO_PATH"])
        )
    elif run_cwd and "{checkout}" in run_cwd:
        raise Exception(
            "Found `{{checkout}}` interpolation in `cwd` for task {name} "
            "but the task doesn't have a checkout: {cwd}".format(
                cwd=run_cwd, name=job.get("name", job.get("label"))
            )
        )

    # dict is for the case of `{'task-reference': text_type}`.
    if isinstance(run_command, (text_type, dict)):
        if is_win:
            if isinstance(run_command, dict):
                for k in run_command.keys():
                    run_command[k] = '"{}"'.format(run_command[k])
            else:
                run_command = '"{}"'.format(run_command)
        run_command = ["bash", "-cx", run_command]

    if run["comm-checkout"]:
        command.append(
            "--comm-checkout={}/comm".format(taskdesc["worker"]["env"]["GECKO_PATH"])
        )

    if run["run-as-root"]:
        command.extend(("--user", "root", "--group", "root"))
    if run_cwd:
        command.extend(("--task-cwd", run_cwd))
    command.append("--")
    if is_bitbar:
        # Use the bitbar wrapper script which sets up the device and adb
        # environment variables
        command.append("/builds/taskcluster/script.py")
    command.extend(run_command)

    if is_win:
        taskref = False
        for c in command:
            if isinstance(c, dict):
                taskref = True

        if taskref:
            cmd = []
            for c in command:
                if isinstance(c, dict):
                    for v in c.values():
                        cmd.append(v)
                else:
                    cmd.append(c)
            worker["command"] = [{"artifact-reference": " ".join(cmd)}]
        else:
            worker["command"] = [" ".join(command)]
    else:
        worker["command"] = [
            ["chmod", "+x", "run-task"],
            command,
        ]
