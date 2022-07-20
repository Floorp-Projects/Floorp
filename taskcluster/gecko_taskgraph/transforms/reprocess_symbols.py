# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the reprocess-symbols task description template,
taskcluster/ci/reprocess-symbols/job-template.yml into an actual task description.
"""


import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.treeherder import inherit_treeherder_from_dep, join_symbol

from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
)

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        assert len(task["dependent-tasks"]) == 2

        build_dep = task["primary-dependency"]
        upload_dep = None
        for dep_idx in task["dependent-tasks"]:
            dep = task["dependent-tasks"][dep_idx]
            if dep_idx != build_dep:
                upload_dep = dep

        task.pop("dependent-tasks", None)

        # Fill out the dynamic fields in the task description
        task["label"] = build_dep.label + "-reprocess-symbols"
        task["dependencies"] = {"build": build_dep.label, "upload": upload_dep.label}
        task["worker"]["env"]["GECKO_HEAD_REPOSITORY"] = config.params[
            "head_repository"
        ]
        task["worker"]["env"]["GECKO_HEAD_REV"] = config.params["head_rev"]
        task["worker"]["env"]["CRASHSTATS_SECRET"] = task["worker"]["env"][
            "CRASHSTATS_SECRET"
        ].format(level=config.params["level"])

        attributes = copy_attributes_from_dependent_job(build_dep)
        attributes.update(task.get("attributes", {}))
        task["attributes"] = attributes

        treeherder = inherit_treeherder_from_dep(task, build_dep)
        th = build_dep.task.get("extra")["treeherder"]
        th_symbol = th.get("symbol")
        th_groupsymbol = th.get("groupSymbol", "?")

        # Disambiguate the treeherder symbol.
        sym = "Rep" + (th_symbol[1:] if th_symbol.startswith("B") else th_symbol)
        treeherder.setdefault("symbol", join_symbol(th_groupsymbol, sym))
        task["treeherder"] = treeherder

        task["run-on-projects"] = build_dep.attributes.get("run_on_projects")
        task["optimization"] = {"reprocess-symbols": None}
        task["if-dependencies"] = ["build"]

        del task["primary-dependency"]

        yield task
