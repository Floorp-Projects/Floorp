# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the upload-symbols task description template,
taskcluster/ci/upload-symbols/job-template.yml into an actual task description.
"""


from gecko_taskgraph.transforms.base import TransformSequence
from gecko_taskgraph.util.attributes import RELEASE_PROJECTS
from gecko_taskgraph.util.treeherder import join_symbol, inherit_treeherder_from_dep
from gecko_taskgraph.util.attributes import copy_attributes_from_dependent_job

import logging

logger = logging.getLogger(__name__)

transforms = TransformSequence()


@transforms.add
def check_nightlies(config, tasks):
    """Ensure that we upload symbols for all shippable builds, so that crash-stats can
    resolve any reports sent to it. Try may enable full symbols but not upload them.

    Putting this check here (instead of the transforms for the build kind) lets us
    leverage the any not-for-build-platforms set in the update-symbols kind."""
    for task in tasks:
        dep = task["primary-dependency"]
        if (
            config.params["project"] in RELEASE_PROJECTS
            and dep.attributes.get("shippable")
            and not dep.attributes.get("enable-full-crashsymbols")
            and not dep.attributes.get("skip-upload-crashsymbols")
        ):
            raise Exception(
                "Shippable job %s should have enable-full-crashsymbols attribute "
                "set to true to enable symbol upload to crash-stats" % dep.label
            )
        yield task


@transforms.add
def fill_template(config, tasks):
    for task in tasks:
        dep = task["primary-dependency"]
        task.pop("dependent-tasks", None)

        # Fill out the dynamic fields in the task description
        task["label"] = dep.label + "-upload-symbols"

        # Skip tasks where we don't have the full crashsymbols enabled
        if not dep.attributes.get("enable-full-crashsymbols") or dep.attributes.get(
            "skip-upload-crashsymbols"
        ):
            logger.debug("Skipping upload symbols task for %s", task["label"])
            continue

        task["dependencies"] = {"build": dep.label}
        task["worker"]["env"]["GECKO_HEAD_REPOSITORY"] = config.params[
            "head_repository"
        ]
        task["worker"]["env"]["GECKO_HEAD_REV"] = config.params["head_rev"]
        task["worker"]["env"]["SYMBOL_SECRET"] = task["worker"]["env"][
            "SYMBOL_SECRET"
        ].format(level=config.params["level"])

        attributes = copy_attributes_from_dependent_job(dep)
        attributes.update(task.get("attributes", {}))
        task["attributes"] = attributes

        treeherder = inherit_treeherder_from_dep(task, dep)
        th = dep.task.get("extra")["treeherder"]
        th_symbol = th.get("symbol")
        th_groupsymbol = th.get("groupSymbol", "?")

        # Disambiguate the treeherder symbol.
        sym = "Sym" + (th_symbol[1:] if th_symbol.startswith("B") else th_symbol)
        treeherder.setdefault("symbol", join_symbol(th_groupsymbol, sym))
        task["treeherder"] = treeherder

        # We only want to run these tasks if the build is run.
        # XXX Better to run this on promote phase instead?
        task["run-on-projects"] = dep.attributes.get("run_on_projects")
        task["optimization"] = {"upload-symbols": None}
        task["if-dependencies"] = ["build"]

        # clear out the stuff that's not part of a task description
        del task["primary-dependency"]

        yield task
