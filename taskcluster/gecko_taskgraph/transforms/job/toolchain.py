# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running toolchain-building jobs via dedicated scripts
"""


import taskgraph
from mozbuild.shellutil import quote as shell_quote

from gecko_taskgraph.util.schema import Schema
from voluptuous import Optional, Required, Any

from gecko_taskgraph.transforms.job import (
    configure_taskdesc_for_run,
    run_job_using,
)
from gecko_taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
)
from gecko_taskgraph.util.hash import hash_paths
from gecko_taskgraph.util.attributes import RELEASE_PROJECTS
from gecko_taskgraph import GECKO


CACHE_TYPE = "toolchains.v3"

toolchain_run_schema = Schema(
    {
        Required("using"): "toolchain-script",
        # The script (in taskcluster/scripts/misc) to run.
        # Python scripts are invoked with `mach python` so vendored libraries
        # are available.
        Required("script"): str,
        # Arguments to pass to the script.
        Optional("arguments"): [str],
        # If not false, tooltool downloads will be enabled via relengAPIProxy
        # for either just public files, or all files.  Not supported on Windows
        Required("tooltool-downloads"): Any(
            False,
            "public",
            "internal",
        ),
        # Sparse profile to give to checkout using `run-task`.  If given,
        # Defaults to "toolchain-build". The value is relative to
        # "sparse-profile-prefix", optionally defined below is the path,
        # defaulting to "build/sparse-profiles".
        # i.e. `build/sparse-profiles/toolchain-build`.
        # If `None`, instructs `run-task` to not use a sparse profile at all.
        Required("sparse-profile"): Any(str, None),
        # The relative path to the sparse profile.
        Optional("sparse-profile-prefix"): str,
        # Paths/patterns pointing to files that influence the outcome of a
        # toolchain build.
        Optional("resources"): [str],
        # Path to the artifact produced by the toolchain job
        Required("toolchain-artifact"): str,
        Optional(
            "toolchain-alias",
            description="An alias that can be used instead of the real toolchain job name in "
            "fetch stanzas for jobs.",
        ): Any(str, [str]),
        Optional(
            "toolchain-env",
            description="Additional env variables to add to the worker when using this toolchain",
        ): {str: object},
        # Base work directory used to set up the task.
        Optional("workdir"): str,
    }
)


def get_digest_data(config, run, taskdesc):
    files = list(run.pop("resources", []))
    # The script
    files.append("taskcluster/scripts/misc/{}".format(run["script"]))
    # Tooltool manifest if any is defined:
    tooltool_manifest = taskdesc["worker"]["env"].get("TOOLTOOL_MANIFEST")
    if tooltool_manifest:
        files.append(tooltool_manifest)

    # Accumulate dependency hashes for index generation.
    data = [hash_paths(GECKO, files)]

    data.append(taskdesc["attributes"]["toolchain-artifact"])

    # If the task uses an in-tree docker image, we want it to influence
    # the index path as well. Ideally, the content of the docker image itself
    # should have an influence, but at the moment, we can't get that
    # information here. So use the docker image name as a proxy. Not a lot of
    # changes to docker images actually have an impact on the resulting
    # toolchain artifact, so we'll just rely on such important changes to be
    # accompanied with a docker image name change.
    image = taskdesc["worker"].get("docker-image", {}).get("in-tree")
    if image:
        data.append(image)

    # Likewise script arguments should influence the index.
    args = run.get("arguments")
    if args:
        data.extend(args)

    if taskdesc["attributes"].get("rebuild-on-release"):
        # Add whether this is a release branch or not
        data.append(str(config.params["project"] in RELEASE_PROJECTS))
    return data


def common_toolchain(config, job, taskdesc, is_docker):
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]
    worker["chain-of-trust"] = True

    if is_docker:
        # If the task doesn't have a docker-image, set a default
        worker.setdefault("docker-image", {"in-tree": "deb11-toolchain-build"})

    # Allow the job to specify where artifacts come from, but add
    # public/build if it's not there already.
    artifacts = worker.setdefault("artifacts", [])
    if not artifacts:
        if is_docker:
            docker_worker_add_artifacts(config, job, taskdesc)
        else:
            generic_worker_add_artifacts(config, job, taskdesc)

    if job["worker"]["os"] == "windows":
        # There were no caches on generic-worker before bug 1519472, and they cause
        # all sorts of problems with Windows toolchain tasks, disable them until
        # tasks are ready.
        run["use-caches"] = False

    env = worker.setdefault("env", {})
    env.update(
        {
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
            "TOOLCHAIN_ARTIFACT": run["toolchain-artifact"],
        }
    )

    if is_docker:
        # Toolchain checkouts don't live under {workdir}/checkouts
        workspace = "{workdir}/workspace/build".format(**run)
        env["GECKO_PATH"] = f"{workspace}/src"

    attributes = taskdesc.setdefault("attributes", {})
    attributes["toolchain-artifact"] = run.pop("toolchain-artifact")
    if "toolchain-alias" in run:
        attributes["toolchain-alias"] = run.pop("toolchain-alias")
    if "toolchain-env" in run:
        attributes["toolchain-env"] = run.pop("toolchain-env")

    digest_data = get_digest_data(config, run, taskdesc)

    if job.get("attributes", {}).get("cached_task") is not False and not taskgraph.fast:
        name = taskdesc["label"].replace(f"{config.kind}-", "", 1)
        taskdesc["cache"] = {
            "type": CACHE_TYPE,
            "name": name,
            "digest-data": digest_data,
        }

    # Toolchains that are used for local development need to be built on a
    # level-3 branch to be installable via `mach bootstrap`.
    if taskdesc["attributes"].get("local-toolchain"):
        if taskdesc.get("run-on-projects"):
            raise Exception(
                "Toolchain {} used for local developement must not have"
                " run-on-projects set".format(taskdesc["label"])
            )
        taskdesc["run-on-projects"] = ["integration", "release"]

    run["using"] = "run-task"
    if is_docker:
        gecko_path = "workspace/build/src"
    elif job["worker"]["os"] == "windows":
        gecko_path = "%GECKO_PATH%"
    else:
        gecko_path = "$GECKO_PATH"

    if is_docker:
        run["cwd"] = run["workdir"]
    run["command"] = [
        "{}/taskcluster/scripts/misc/{}".format(gecko_path, run.pop("script"))
    ] + run.pop("arguments", [])
    if not is_docker:
        # Don't quote the first item in the command because it purposely contains
        # an environment variable that is not meant to be quoted.
        if len(run["command"]) > 1:
            run["command"] = run["command"][0] + " " + shell_quote(*run["command"][1:])
        else:
            run["command"] = run["command"][0]

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])


toolchain_defaults = {
    "tooltool-downloads": False,
    "sparse-profile": "toolchain-build",
}


@run_job_using(
    "docker-worker",
    "toolchain-script",
    schema=toolchain_run_schema,
    defaults=toolchain_defaults,
)
def docker_worker_toolchain(config, job, taskdesc):
    common_toolchain(config, job, taskdesc, is_docker=True)


@run_job_using(
    "generic-worker",
    "toolchain-script",
    schema=toolchain_run_schema,
    defaults=toolchain_defaults,
)
def generic_worker_toolchain(config, job, taskdesc):
    common_toolchain(config, job, taskdesc, is_docker=False)
