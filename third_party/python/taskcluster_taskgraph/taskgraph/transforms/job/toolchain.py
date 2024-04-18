# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running toolchain-building jobs via dedicated scripts
"""

from voluptuous import ALLOW_EXTRA, Any, Optional, Required

import taskgraph
from taskgraph.transforms.job import configure_taskdesc_for_run, run_job_using
from taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
    get_vcsdir_name,
)
from taskgraph.util.hash import hash_paths
from taskgraph.util.schema import Schema
from taskgraph.util.shell import quote as shell_quote

CACHE_TYPE = "toolchains.v3"

toolchain_run_schema = Schema(
    {
        Required("using"): "toolchain-script",
        # The script (in taskcluster/scripts/misc) to run.
        Required("script"): str,
        # Arguments to pass to the script.
        Optional("arguments"): [str],
        # Sparse profile to give to checkout using `run-task`.  If given,
        # a filename in `build/sparse-profiles`.  Defaults to
        # "toolchain-build", i.e., to
        # `build/sparse-profiles/toolchain-build`.  If `None`, instructs
        # `run-task` to not use a sparse profile at all.
        Required("sparse-profile"): Any(str, None),
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
        Required("workdir"): str,
    },
    extra=ALLOW_EXTRA,
)


def get_digest_data(config, run, taskdesc):
    files = list(run.pop("resources", []))
    # The script
    files.append("taskcluster/scripts/toolchain/{}".format(run["script"]))

    # Accumulate dependency hashes for index generation.
    data = [hash_paths(config.graph_config.vcs_root, files)]

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
    return data


def common_toolchain(config, job, taskdesc, is_docker):
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]
    worker["chain-of-trust"] = True

    srcdir = get_vcsdir_name(worker["os"])

    if is_docker:
        # If the task doesn't have a docker-image, set a default
        worker.setdefault("docker-image", {"in-tree": "toolchain-build"})

    # Allow the job to specify where artifacts come from, but add
    # public/build if it's not there already.
    artifacts = worker.setdefault("artifacts", [])
    if not any(artifact.get("name") == "public/build" for artifact in artifacts):
        if is_docker:
            docker_worker_add_artifacts(config, job, taskdesc)
        else:
            generic_worker_add_artifacts(config, job, taskdesc)

    env = worker["env"]
    env.update(
        {
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
        }
    )

    attributes = taskdesc.setdefault("attributes", {})
    attributes["toolchain-artifact"] = run.pop("toolchain-artifact")
    if "toolchain-alias" in run:
        attributes["toolchain-alias"] = run.pop("toolchain-alias")
    if "toolchain-env" in run:
        attributes["toolchain-env"] = run.pop("toolchain-env")

    if not taskgraph.fast:
        name = taskdesc["label"].replace(f"{config.kind}-", "", 1)
        taskdesc["cache"] = {
            "type": CACHE_TYPE,
            "name": name,
            "digest-data": get_digest_data(config, run, taskdesc),
        }

    script = run.pop("script")
    run["using"] = "run-task"
    run["cwd"] = "{checkout}/.."

    if script.endswith(".ps1"):
        run["exec-with"] = "powershell"

    command = [f"{srcdir}/taskcluster/scripts/toolchain/{script}"] + run.pop(
        "arguments", []
    )

    if not is_docker:
        # Don't quote the first item in the command because it purposely contains
        # an environment variable that is not meant to be quoted.
        if len(command) > 1:
            command = command[0] + " " + shell_quote(*command[1:])
        else:
            command = command[0]

    run["command"] = command

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])


toolchain_defaults = {
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
