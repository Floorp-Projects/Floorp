# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running spidermonkey jobs via dedicated scripts
"""


from taskgraph.util.schema import Schema
from voluptuous import Required, Any, Optional

from gecko_taskgraph.transforms.job import (
    run_job_using,
    configure_taskdesc_for_run,
)
from gecko_taskgraph.transforms.job.common import (
    docker_worker_add_artifacts,
    generic_worker_add_artifacts,
)

sm_run_schema = Schema(
    {
        Required("using"): Any(
            "spidermonkey",
            "spidermonkey-package",
        ),
        # SPIDERMONKEY_VARIANT and SPIDERMONKEY_PLATFORM
        Required("spidermonkey-variant"): str,
        Optional("spidermonkey-platform"): str,
        # Base work directory used to set up the task.
        Optional("workdir"): str,
        Required("tooltool-downloads"): Any(
            False,
            "public",
            "internal",
        ),
    }
)


@run_job_using("docker-worker", "spidermonkey", schema=sm_run_schema)
@run_job_using("docker-worker", "spidermonkey-package", schema=sm_run_schema)
def docker_worker_spidermonkey(config, job, taskdesc):
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]
    worker.setdefault("artifacts", [])

    docker_worker_add_artifacts(config, job, taskdesc)

    env = worker.setdefault("env", {})
    env.update(
        {
            "MOZHARNESS_DISABLE": "true",
            "SPIDERMONKEY_VARIANT": run.pop("spidermonkey-variant"),
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
        }
    )
    if "spidermonkey-platform" in run:
        env["SPIDERMONKEY_PLATFORM"] = run.pop("spidermonkey-platform")

    script = "build-sm.sh"
    if run["using"] == "spidermonkey-package":
        script = "build-sm-package.sh"

    run["using"] = "run-task"
    run["cwd"] = run["workdir"]
    run["command"] = [f"./checkouts/gecko/taskcluster/scripts/builder/{script}"]

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])


@run_job_using("generic-worker", "spidermonkey", schema=sm_run_schema)
def generic_worker_spidermonkey(config, job, taskdesc):
    assert job["worker"]["os"] == "windows", "only supports windows right now"

    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]

    generic_worker_add_artifacts(config, job, taskdesc)

    env = worker.setdefault("env", {})
    env.update(
        {
            "MOZHARNESS_DISABLE": "true",
            "SPIDERMONKEY_VARIANT": run.pop("spidermonkey-variant"),
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
            "SCCACHE_DISABLE": "1",
            "WORK": ".",  # Override the defaults in build scripts
            "GECKO_PATH": "./src",  # with values suiteable for windows generic worker
            "UPLOAD_DIR": "./public/build",
        }
    )
    if "spidermonkey-platform" in run:
        env["SPIDERMONKEY_PLATFORM"] = run.pop("spidermonkey-platform")

    script = "build-sm.sh"
    if run["using"] == "spidermonkey-package":
        script = "build-sm-package.sh"
        # Don't allow untested configurations yet
        raise Exception("spidermonkey-package is not a supported configuration")

    run["using"] = "run-task"
    run["command"] = [
        "c:\\mozilla-build\\msys\\bin\\bash.exe "  # string concat
        '"./src/taskcluster/scripts/builder/%s"' % script
    ]

    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])
