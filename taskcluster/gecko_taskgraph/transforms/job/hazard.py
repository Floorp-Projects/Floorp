# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Support for running hazard jobs via dedicated scripts
"""


from taskgraph.util.schema import Schema
from voluptuous import Required, Optional, Any

from gecko_taskgraph.transforms.job import (
    run_job_using,
    configure_taskdesc_for_run,
)
from gecko_taskgraph.transforms.job.common import (
    setup_secrets,
    docker_worker_add_artifacts,
    add_tooltool,
)

haz_run_schema = Schema(
    {
        Required("using"): "hazard",
        # The command to run within the task image (passed through to the worker)
        Required("command"): str,
        # The mozconfig to use; default in the script is used if omitted
        Optional("mozconfig"): str,
        # The set of secret names to which the task has access; these are prefixed
        # with `project/releng/gecko/{treeherder.kind}/level-{level}/`.   Setting
        # this will enable any worker features required and set the task's scopes
        # appropriately.  `true` here means ['*'], all secrets.  Not supported on
        # Windows
        Optional("secrets"): Any(bool, [str]),
        # Base work directory used to set up the task.
        Optional("workdir"): str,
    }
)


@run_job_using("docker-worker", "hazard", schema=haz_run_schema)
def docker_worker_hazard(config, job, taskdesc):
    run = job["run"]

    worker = taskdesc["worker"] = job["worker"]
    worker.setdefault("artifacts", [])

    docker_worker_add_artifacts(config, job, taskdesc)
    worker.setdefault("required-volumes", []).append(
        "{workdir}/workspace".format(**run)
    )
    add_tooltool(config, job, taskdesc)
    setup_secrets(config, job, taskdesc)

    env = worker["env"]
    env.update(
        {
            "MOZ_BUILD_DATE": config.params["moz_build_date"],
            "MOZ_SCM_LEVEL": config.params["level"],
        }
    )

    # script parameters
    if run.get("mozconfig"):
        env["MOZCONFIG"] = run.pop("mozconfig")

    run["using"] = "run-task"
    run["cwd"] = run["workdir"]
    configure_taskdesc_for_run(config, job, taskdesc, worker["implementation"])
