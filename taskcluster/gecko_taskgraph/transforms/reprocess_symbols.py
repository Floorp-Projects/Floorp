# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the reprocess-symbols task description template,
taskcluster/ci/reprocess-symbols/job-template.yml into an actual task description.
"""


import logging

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.dependencies import get_dependencies, get_primary_dependency
from taskgraph.util.treeherder import inherit_treeherder_from_dep, join_symbol

from gecko_taskgraph.util.attributes import (
    copy_attributes_from_dependent_job,
    sorted_unique_list,
)

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def gather_required_signoffs(config, jobs):
    for job in jobs:
        job.setdefault("attributes", {})["required_signoffs"] = sorted_unique_list(
            *(
                dep.attributes.get("required_signoffs", [])
                for dep in get_dependencies(config, job)
            )
        )
        yield job


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        dep = get_primary_dependency(config, task)
        assert dep

        # Fill out the dynamic fields in the task description
        task["label"] = dep.label + "-reprocess-symbols"
        task["worker"]["env"]["GECKO_HEAD_REPOSITORY"] = config.params[
            "head_repository"
        ]
        task["worker"]["env"]["GECKO_HEAD_REV"] = config.params["head_rev"]
        task["worker"]["env"]["CRASHSTATS_SECRET"] = task["worker"]["env"][
            "CRASHSTATS_SECRET"
        ].format(level=config.params["level"])

        attributes = copy_attributes_from_dependent_job(dep)
        attributes.update(task.get("attributes", {}))
        task["attributes"] = attributes

        treeherder = inherit_treeherder_from_dep(task, dep)
        th = dep.task["extra"]["treeherder"]
        th_symbol = th.get("symbol")
        th_groupsymbol = th.get("groupSymbol", "?")

        # Disambiguate the treeherder symbol.
        sym = "Rep" + (th_symbol[1:] if th_symbol.startswith("B") else th_symbol)
        treeherder.setdefault("symbol", join_symbol(th_groupsymbol, sym))
        task["treeherder"] = treeherder

        task["run-on-projects"] = dep.attributes.get("run_on_projects")
        task["optimization"] = {"reprocess-symbols": None}
        task["if-dependencies"] = [task["attributes"]["primary-kind-dependency"]]

        yield task
