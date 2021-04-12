# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running toolchain-building jobs via dedicated scripts
"""

from __future__ import absolute_import, print_function, unicode_literals

from mozbuild.shellutil import quote as shell_quote

from six import text_type
from taskgraph.util.schema import Schema
from voluptuous import Optional, Required, Any

from taskgraph.transforms.job import (
    configure_taskdesc_for_run,
    run_job_using,
)
from taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
)
from taskgraph.util.hash import hash_paths
from taskgraph.util.attributes import RELEASE_PROJECTS
from taskgraph import GECKO
import taskgraph


CACHE_TYPE = "toolchains.v3"

toolchain_run_schema = Schema(
    {
        Required("using"): "toolchain-script",
        # The script (in taskcluster/scripts/misc) to run.
        # Python scripts are invoked with `mach python` so vendored libraries
        # are available.
        Required("script"): text_type,
        # Arguments to pass to the script.
        Optional("arguments"): [text_type],
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
        Required("sparse-profile"): Any(text_type, None),
        # The relative path to the sparse profile.
        Optional("sparse-profile-prefix"): text_type,
        # Paths/patterns pointing to files that influence the outcome of a
        # toolchain build.
        Optional("resources"): [text_type],
        # Path to the artifact produced by the toolchain job
        Required("toolchain-artifact"): text_type,
        Optional(
            "toolchain-alias",
            description="An alias that can be used instead of the real toolchain job name in "
            "fetch stanzas for jobs.",
        ): Any(text_type, [text_type]),
        # Base work directory used to set up the task.
        Optional("workdir"): text_type,
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
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]
    worker["chain-of-trust"] = True

    # If the task doesn't have a docker-image, set a default
    worker.setdefault("docker-image", {"in-tree": "deb10-toolchain-build"})

    # Allow the job to specify where artifacts come from, but add
    # public/build if it's not there already.
    artifacts = worker.setdefault("artifacts", [])
    if not artifacts:
        docker_worker_add_artifacts(config, job, taskdesc)

    # Toolchain checkouts don't live under {workdir}/checkouts
    workspace = "{workdir}/workspace/build".format(**run)
    gecko_path = "{}/src".format(workspace)

    env = worker.setdefault("env", {})
    env.update(
        {
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
            "GECKO_PATH": gecko_path,
        }
    )

    attributes = taskdesc.setdefault("attributes", {})
    attributes["toolchain-artifact"] = run.pop("toolchain-artifact")
    if "toolchain-alias" in run:
        attributes["toolchain-alias"] = run.pop("toolchain-alias")

    if not taskgraph.fast:
        name = taskdesc["label"].replace("{}-".format(config.kind), "", 1)
        taskdesc["cache"] = {
            "type": CACHE_TYPE,
            "name": name,
            "digest-data": get_digest_data(config, run, taskdesc),
        }

    run["using"] = "run-task"
    run["cwd"] = run["workdir"]
    run["command"] = [
        "workspace/build/src/taskcluster/scripts/misc/{}".format(run.pop("script"))
    ] + run.pop("arguments", [])

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])


@run_job_using(
    "generic-worker",
    "toolchain-script",
    schema=toolchain_run_schema,
    defaults=toolchain_defaults,
)
def generic_worker_toolchain(config, job, taskdesc):
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]
    worker["chain-of-trust"] = True

    # Allow the job to specify where artifacts come from, but add
    # public/build if it's not there already.
    artifacts = worker.setdefault("artifacts", [])
    if not artifacts:
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
        }
    )

    # Use `mach` to invoke python scripts so in-tree libraries are available.
    if run["script"].endswith(".py"):
        raise NotImplementedError(
            "Python toolchain scripts aren't supported on generic-worker"
        )

    attributes = taskdesc.setdefault("attributes", {})
    attributes["toolchain-artifact"] = run.pop("toolchain-artifact")
    if "toolchain-alias" in run:
        attributes["toolchain-alias"] = run.pop("toolchain-alias")

    if not taskgraph.fast:
        name = taskdesc["label"].replace("{}-".format(config.kind), "", 1)
        taskdesc["cache"] = {
            "type": CACHE_TYPE,
            "name": name,
            "digest-data": get_digest_data(config, run, taskdesc),
        }

    run["using"] = "run-task"

    args = run.pop("arguments", "")
    if args:
        args = " " + shell_quote(*args)

    if job["worker"]["os"] == "windows":
        gecko_path = "%GECKO_PATH%"
    else:
        gecko_path = "$GECKO_PATH"

    run["command"] = "{}/taskcluster/scripts/misc/{}{}".format(
        gecko_path, run.pop("script"), args
    )

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])
