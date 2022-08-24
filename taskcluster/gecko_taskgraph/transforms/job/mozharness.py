# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""

Support for running jobs via mozharness.  Ideally, most stuff gets run this
way, and certainly anything using mozharness should use this approach.

"""

import json

from textwrap import dedent

from taskgraph.util.schema import Schema
from voluptuous import Required, Optional, Any
from voluptuous.validators import Match

from mozpack import path as mozpath

from gecko_taskgraph.transforms.job import (
    configure_taskdesc_for_run,
    run_job_using,
)
from gecko_taskgraph.transforms.job.common import (
    setup_secrets,
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
    get_expiration,
)
from gecko_taskgraph.transforms.task import (
    get_branch_repo,
    get_branch_rev,
)
from gecko_taskgraph.util.attributes import is_try

mozharness_run_schema = Schema(
    {
        Required("using"): "mozharness",
        # the mozharness script used to run this task, relative to the testing/
        # directory and using forward slashes even on Windows
        Required("script"): str,
        # Additional paths to look for mozharness configs in. These should be
        # relative to the base of the source checkout
        Optional("config-paths"): [str],
        # the config files required for the task, relative to
        # testing/mozharness/configs or one of the paths specified in
        # `config-paths` and using forward slashes even on Windows
        Required("config"): [str],
        # any additional actions to pass to the mozharness command
        Optional("actions"): [
            Match("^[a-z0-9-]+$", "actions must be `-` seperated alphanumeric strings")
        ],
        # any additional options (without leading --) to be passed to mozharness
        Optional("options"): [
            Match(
                "^[a-z0-9-]+(=[^ ]+)?$",
                "options must be `-` seperated alphanumeric strings (with optional argument)",
            )
        ],
        # --custom-build-variant-cfg value
        Optional("custom-build-variant-cfg"): str,
        # Extra configuration options to pass to mozharness.
        Optional("extra-config"): dict,
        # If not false, tooltool downloads will be enabled via relengAPIProxy
        # for either just public files, or all files.  Not supported on Windows
        Required("tooltool-downloads"): Any(
            False,
            "public",
            "internal",
        ),
        # The set of secret names to which the task has access; these are prefixed
        # with `project/releng/gecko/{treeherder.kind}/level-{level}/`.  Setting
        # this will enable any worker features required and set the task's scopes
        # appropriately.  `true` here means ['*'], all secrets.  Not supported on
        # Windows
        Required("secrets"): Any(bool, [str]),
        # If true, taskcluster proxy will be enabled; note that it may also be enabled
        # automatically e.g., for secrets support.  Not supported on Windows.
        Required("taskcluster-proxy"): bool,
        # If true, the build scripts will start Xvfb.  Not supported on Windows.
        Required("need-xvfb"): bool,
        # If false, indicate that builds should skip producing artifacts.  Not
        # supported on Windows.
        Required("keep-artifacts"): bool,
        # If specified, use the in-tree job script specified.
        Optional("job-script"): str,
        Required("requires-signed-builds"): bool,
        # Whether or not to use caches.
        Optional("use-caches"): bool,
        # If false, don't set MOZ_SIMPLE_PACKAGE_NAME
        # Only disableable on windows
        Required("use-simple-package"): bool,
        # If false don't pass --branch mozharness script
        # Only disableable on windows
        Required("use-magic-mh-args"): bool,
        # if true, perform a checkout of a comm-central based branch inside the
        # gecko checkout
        Required("comm-checkout"): bool,
        # Base work directory used to set up the task.
        Optional("workdir"): str,
    }
)


mozharness_defaults = {
    "tooltool-downloads": False,
    "secrets": False,
    "taskcluster-proxy": False,
    "need-xvfb": False,
    "keep-artifacts": True,
    "requires-signed-builds": False,
    "use-simple-package": True,
    "use-magic-mh-args": True,
    "comm-checkout": False,
}


@run_job_using(
    "docker-worker",
    "mozharness",
    schema=mozharness_run_schema,
    defaults=mozharness_defaults,
)
def mozharness_on_docker_worker_setup(config, job, taskdesc):
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]

    if not run.pop("use-simple-package", None):
        raise NotImplementedError(
            "Simple packaging cannot be disabled via"
            "'use-simple-package' on docker-workers"
        )
    if not run.pop("use-magic-mh-args", None):
        raise NotImplementedError(
            "Cannot disabled mh magic arg passing via"
            "'use-magic-mh-args' on docker-workers"
        )

    # Running via mozharness assumes an image that contains build.sh:
    # by default, debian11-amd64-build, but it could be another image (like
    # android-build).
    worker.setdefault("docker-image", {"in-tree": "debian11-amd64-build"})

    worker.setdefault("artifacts", []).append(
        {
            "name": "public/logs",
            "path": "{workdir}/logs/".format(**run),
            "type": "directory",
            "expires-after": get_expiration(config, "medium"),
        }
    )
    worker["taskcluster-proxy"] = run.pop("taskcluster-proxy", None)
    docker_worker_add_artifacts(config, job, taskdesc)

    env = worker.setdefault("env", {})
    env.update(
        {
            "WORKSPACE": "{workdir}/workspace".format(**run),
            "MOZHARNESS_CONFIG": " ".join(run.pop("config")),
            "MOZHARNESS_SCRIPT": run.pop("script"),
            "MH_BRANCH": config.params["project"],
            "MOZ_SOURCE_CHANGESET": get_branch_rev(config),
            "MOZ_SOURCE_REPO": get_branch_repo(config),
            "MH_BUILD_POOL": "taskcluster",
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
            "PYTHONUNBUFFERED": "1",
        }
    )

    worker.setdefault("required-volumes", []).append(env["WORKSPACE"])

    if "actions" in run:
        env["MOZHARNESS_ACTIONS"] = " ".join(run.pop("actions"))

    if "options" in run:
        env["MOZHARNESS_OPTIONS"] = " ".join(run.pop("options"))

    if "config-paths" in run:
        env["MOZHARNESS_CONFIG_PATHS"] = " ".join(run.pop("config-paths"))

    if "custom-build-variant-cfg" in run:
        env["MH_CUSTOM_BUILD_VARIANT_CFG"] = run.pop("custom-build-variant-cfg")

    extra_config = run.pop("extra-config", {})
    extra_config["objdir"] = "obj-build"
    env["EXTRA_MOZHARNESS_CONFIG"] = json.dumps(extra_config, sort_keys=True)

    if "job-script" in run:
        env["JOB_SCRIPT"] = run["job-script"]

    if is_try(config.params):
        env["TRY_COMMIT_MSG"] = config.params["message"]

    # if we're not keeping artifacts, set some env variables to empty values
    # that will cause the build process to skip copying the results to the
    # artifacts directory.  This will have no effect for operations that are
    # not builds.
    if not run.pop("keep-artifacts"):
        env["DIST_TARGET_UPLOADS"] = ""
        env["DIST_UPLOADS"] = ""

    # Xvfb
    if run.pop("need-xvfb"):
        env["NEED_XVFB"] = "true"
    else:
        env["NEED_XVFB"] = "false"

    # Retry if mozharness returns TBPL_RETRY
    worker["retry-exit-status"] = [4]

    setup_secrets(config, job, taskdesc)

    run["using"] = "run-task"
    run["command"] = mozpath.join(
        "${GECKO_PATH}",
        run.pop("job-script", "taskcluster/scripts/builder/build-linux.sh"),
    )
    run.pop("secrets")
    run.pop("requires-signed-builds")

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])


@run_job_using(
    "generic-worker",
    "mozharness",
    schema=mozharness_run_schema,
    defaults=mozharness_defaults,
)
def mozharness_on_generic_worker(config, job, taskdesc):
    assert job["worker"]["os"] in (
        "windows",
        "macosx",
    ), "only supports windows and macOS right now: {}".format(job["label"])

    run = job["run"]

    # fail if invalid run options are included
    invalid = []
    for prop in ["need-xvfb"]:
        if prop in run and run.pop(prop):
            invalid.append(prop)
    if not run.pop("keep-artifacts", True):
        invalid.append("keep-artifacts")
    if invalid:
        raise Exception(
            "Jobs run using mozharness on Windows do not support properties "
            + ", ".join(invalid)
        )

    worker = taskdesc["worker"] = job["worker"]

    worker["taskcluster-proxy"] = run.pop("taskcluster-proxy", None)

    setup_secrets(config, job, taskdesc)

    taskdesc["worker"].setdefault("artifacts", []).append(
        {
            "name": "public/logs",
            "path": "logs",
            "type": "directory",
            "expires-after": get_expiration(config, "medium"),
        }
    )

    if not worker.get("skip-artifacts", False):
        generic_worker_add_artifacts(config, job, taskdesc)

    env = worker.setdefault("env", {})
    env.update(
        {
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
            "MH_BRANCH": config.params["project"],
            "MOZ_SOURCE_CHANGESET": get_branch_rev(config),
            "MOZ_SOURCE_REPO": get_branch_repo(config),
        }
    )
    if run.pop("use-simple-package"):
        env.update({"MOZ_SIMPLE_PACKAGE_NAME": "target"})

    extra_config = run.pop("extra-config", {})
    extra_config["objdir"] = "obj-build"
    env["EXTRA_MOZHARNESS_CONFIG"] = json.dumps(extra_config, sort_keys=True)

    # The windows generic worker uses batch files to pass environment variables
    # to commands.  Setting a variable to empty in a batch file unsets, so if
    # there is no `TRY_COMMIT_MESSAGE`, pass a space instead, so that
    # mozharness doesn't try to find the commit message on its own.
    if is_try(config.params):
        env["TRY_COMMIT_MSG"] = config.params["message"] or "no commit message"

    if not job["attributes"]["build_platform"].startswith(("win", "macosx")):
        raise Exception(
            "Task generation for mozharness build jobs currently only supported on "
            "Windows and macOS"
        )

    mh_command = []
    if job["worker"]["os"] == "windows":
        mh_command.append("c:/mozilla-build/python3/python3.exe")
        gecko_path = "%GECKO_PATH%"
    else:
        gecko_path = "$GECKO_PATH"

    mh_command += [
        f"{gecko_path}/mach",
        "python",
        "{}/testing/{}".format(gecko_path, run.pop("script")),
    ]

    for path in run.pop("config-paths", []):
        mh_command.append(f"--extra-config-path {gecko_path}/{path}")

    for cfg in run.pop("config"):
        mh_command.extend(("--config", cfg))
    if run.pop("use-magic-mh-args"):
        mh_command.extend(("--branch", config.params["project"]))
    if job["worker"]["os"] == "windows":
        mh_command.extend(("--work-dir", r"%cd:Z:=z:%\workspace"))
    for action in run.pop("actions", []):
        mh_command.append("--" + action)

    for option in run.pop("options", []):
        mh_command.append("--" + option)
    if run.get("custom-build-variant-cfg"):
        mh_command.append("--custom-build-variant")
        mh_command.append(run.pop("custom-build-variant-cfg"))

    if job["worker"]["os"] == "macosx":
        # Ideally, we'd use shellutil.quote, but that would single-quote
        # $GECKO_PATH, which would defeat having the variable in the command
        # in the first place, as it wouldn't be expanded.
        # In practice, arguments are expected not to contain characters that
        # would require quoting.
        mh_command = " ".join(mh_command)

    run["using"] = "run-task"
    run["command"] = mh_command
    run.pop("secrets")
    run.pop("requires-signed-builds")
    run.pop("job-script", None)
    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])

    # Everything past this point is Windows-specific.
    if job["worker"]["os"] == "macosx":
        return

    if taskdesc.get("use-sccache"):
        worker["command"] = (
            [
                # Make the comment part of the first command, as it will help users to
                # understand what is going on, and why these steps are implemented.
                dedent(
                    """\
            :: sccache currently uses the full compiler commandline as input to the
            :: cache hash key, so create a symlink to the task dir and build from
            :: the symlink dir to get consistent paths.
            if exist z:\\build rmdir z:\\build"""
                ),
                r"mklink /d z:\build %cd%",
                # Grant delete permission on the link to everyone.
                r"icacls z:\build /grant *S-1-1-0:D /L",
                r"cd /d z:\build",
            ]
            + worker["command"]
        )
