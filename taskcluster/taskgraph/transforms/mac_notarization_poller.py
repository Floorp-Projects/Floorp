# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Transform the mac notarization poller task into an actual task description.
"""

from __future__ import absolute_import, print_function, unicode_literals

from taskgraph.transforms.base import TransformSequence
from taskgraph.util.attributes import copy_attributes_from_dependent_job
from taskgraph.util.taskcluster import get_artifact_url
from taskgraph.util.treeherder import add_suffix, join_symbol


transforms = TransformSequence()


@transforms.add
def make_task_description(config, jobs):
    for job in jobs:
        dep_job = job["primary-dependency"]
        attributes = dep_job.attributes

        build_platform = dep_job.attributes.get("build_platform")
        treeherder = None
        if "partner" not in config.kind and "eme-free" not in config.kind:
            treeherder = job.get("treeherder", {})

            dep_th_platform = (
                dep_job.task.get("extra", {})
                .get("treeherder", {})
                .get("machine", {})
                .get("platform", "")
            )
            build_type = dep_job.attributes.get("build_type")
            treeherder.setdefault(
                "platform", "{}/{}".format(dep_th_platform, build_type)
            )

            dep_treeherder = dep_job.task.get("extra", {}).get("treeherder", {})
            treeherder.setdefault("tier", dep_treeherder.get("tier", 1))
            treeherder.setdefault(
                "symbol",
                _generate_treeherder_symbol(
                    dep_treeherder.get("groupSymbol", "?"), dep_treeherder.get("symbol")
                ),
            )
            treeherder.setdefault("kind", "build")

        label = dep_job.label.replace("part-1", "poller")
        description = (
            "Mac Notarization Poller for build '"
            "{build_platform}/{build_type}'".format(
                build_platform=build_platform, build_type=attributes.get("build_type")
            )
        )

        attributes = (
            job["attributes"]
            if job.get("attributes")
            else copy_attributes_from_dependent_job(dep_job)
        )
        attributes["signed"] = True

        if dep_job.attributes.get("chunk_locales"):
            # Used for l10n attribute passthrough
            attributes["chunk_locales"] = dep_job.attributes.get("chunk_locales")

        uuid_manifest_url = get_artifact_url("<part1>", "public/uuid_manifest.json")
        task = {
            "label": label,
            "description": description,
            "worker": {
                "implementation": "notarization-poller",
                "uuid-manifest": {"task-reference": uuid_manifest_url},
                "max-run-time": 3600,
            },
            "worker-type": "mac-notarization-poller",
            "dependencies": {"part1": dep_job.label},
            "attributes": attributes,
            "run-on-projects": dep_job.attributes.get("run_on_projects"),
            "optimization": dep_job.optimization,
            "routes": job.get("routes", []),
            "shipping-product": job.get("shipping-product"),
            "shipping-phase": job.get("shipping-phase"),
        }

        if treeherder:
            task["treeherder"] = treeherder
        if job.get("extra"):
            task["extra"] = job["extra"]
        # we may have reduced the priority for partner jobs, otherwise task.py will set it
        if job.get("priority"):
            task["priority"] = job["priority"]

        yield task


def _generate_treeherder_symbol(group_symbol, build_symbol):
    return join_symbol(group_symbol, add_suffix(build_symbol, "-poll"))
