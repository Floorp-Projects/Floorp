# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
"""
Generate labels for tasks without names, consistently.
Uses attributes from `primary-dependency`.
"""

from taskgraph.transforms.base import TransformSequence

transforms = TransformSequence()

SYMBOL = "{groupSymbol}({symbol}-vismet)"
# the test- prefix makes the task SETA-optimized.
LABEL = "test-vismet-{platform}-{label}"


@transforms.add
def make_label(config, jobs):
    """Generate a sane label for a new task constructed from a dependency
    Using attributes from the dependent job and the current task kind"""
    for job in jobs:
        dep_job = job["primary-dependency"]
        attr = dep_job.attributes.get

        if attr("locale", job.get("locale")):
            template = "{kind}-{locale}-{build_platform}/{build_type}"
        elif attr("l10n_chunk"):
            template = "{kind}-{build_platform}-{l10n_chunk}/{build_type}"
        elif config.kind.startswith("release-eme-free") or config.kind.startswith(
            "release-partner-repack"
        ):
            suffix = job.get("extra", {}).get("repack_suffix", None) or job.get(
                "extra", {}
            ).get("repack_id", None)
            template = "{kind}-{build_platform}"
            if suffix:
                template += "-{}".format(suffix.replace("/", "-"))
        else:
            template = "{kind}-{build_platform}/{build_type}"
        job["label"] = template.format(
            kind=config.kind,
            build_platform=attr("build_platform"),
            build_type=attr("build_type"),
            locale=attr("locale", job.get("locale", "")),  # Locale can be absent
            l10n_chunk=attr("l10n_chunk", ""),  # Can be empty
        )

        yield job


@transforms.add
def run_visual_metrics(config, jobs):
    for job in jobs:
        dep_job = job.pop("primary-dependency", None)
        if dep_job is not None:
            platform = dep_job.task["extra"]["treeherder-platform"]
            job["dependencies"] = {dep_job.label: dep_job.label}

            # Add the artifact to be processed as a fetches artifact
            job["fetches"][dep_job.label] = [
                {"artifact": "browsertime-results.tgz", "extract": True}
            ]

            # vismet runs on Linux but we want to have it displayed
            # alongside the job it was triggered by to make it easier for
            # people to find it back.
            job["label"] = LABEL.format(platform=platform, label=dep_job.label)
            treeherder_info = dict(dep_job.task["extra"]["treeherder"])
            job["treeherder"]["platform"] = platform
            job["treeherder"]["symbol"] = SYMBOL.format(
                groupSymbol=treeherder_info["groupSymbol"],
                symbol=treeherder_info["symbol"],
            )

            # Store the platform name so we can use it to calculate
            # the similarity metric against other tasks
            job["worker"].setdefault("env", {})["TC_PLATFORM"] = platform

            # run-on-projects needs to be set based on the dependent task
            attributes = dict(dep_job.attributes)
            job["run-on-projects"] = attributes["run_on_projects"]

            # The run-on-tasks-for also needs to be setup here
            job["run-on-tasks-for"] = attributes.get("run_on_tasks_for", [])

            # We can't use the multi_dep transforms which remove this
            # field, so we remove the dependent-tasks entry here
            del job["dependent-tasks"]

            yield job
