# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import re

INTEGRATION_PROJECTS = {
    "autoland",
}

TRUNK_PROJECTS = INTEGRATION_PROJECTS | {"mozilla-central", "comm-central"}

RELEASE_PROJECTS = {
    "mozilla-central",
    "mozilla-beta",
    "mozilla-release",
    "mozilla-esr115",
    "comm-central",
    "comm-beta",
    "comm-release",
    "comm-esr115",
    # bug 1845368: pine is a permanent project branch used for testing
    # nightly updates
    "pine",
}

RELEASE_PROMOTION_PROJECTS = {
    "jamun",
    "maple",
    "try",
    "try-comm-central",
} | RELEASE_PROJECTS

TEMPORARY_PROJECTS = set(
    {
        # When using a "Disposable Project Branch" you can specify your branch here. e.g.:
        "oak",
    }
)

TRY_PROJECTS = {
    "try",
    "try-comm-central",
}

ALL_PROJECTS = RELEASE_PROMOTION_PROJECTS | TRUNK_PROJECTS | TEMPORARY_PROJECTS

RUN_ON_PROJECT_ALIASES = {
    # key is alias, value is lambda to test it against
    "all": lambda project: True,
    "integration": lambda project: (
        project in INTEGRATION_PROJECTS or project == "toolchains"
    ),
    "release": lambda project: (project in RELEASE_PROJECTS or project == "toolchains"),
    "trunk": lambda project: (project in TRUNK_PROJECTS or project == "toolchains"),
    "trunk-only": lambda project: project in TRUNK_PROJECTS,
    "autoland": lambda project: project in ("autoland", "toolchains"),
    "autoland-only": lambda project: project == "autoland",
    "mozilla-central": lambda project: project in ("mozilla-central", "toolchains"),
    "mozilla-central-only": lambda project: project == "mozilla-central",
}

_COPYABLE_ATTRIBUTES = (
    "accepted-mar-channel-ids",
    "artifact_map",
    "artifact_prefix",
    "build_platform",
    "build_type",
    "l10n_chunk",
    "locale",
    "mar-channel-id",
    "maven_packages",
    "nightly",
    "required_signoffs",
    "shippable",
    "shipping_phase",
    "shipping_product",
    "signed",
    "stub-installer",
    "update-channel",
)


def match_run_on_projects(project, run_on_projects):
    """Determine whether the given project is included in the `run-on-projects`
    parameter, applying expansions for things like "integration" mentioned in
    the attribute documentation."""
    aliases = RUN_ON_PROJECT_ALIASES.keys()
    run_aliases = set(aliases) & set(run_on_projects)
    if run_aliases:
        if any(RUN_ON_PROJECT_ALIASES[alias](project) for alias in run_aliases):
            return True

    return project in run_on_projects


def match_run_on_hg_branches(hg_branch, run_on_hg_branches):
    """Determine whether the given project is included in the `run-on-hg-branches`
    parameter. Allows 'all'."""
    if "all" in run_on_hg_branches:
        return True

    for expected_hg_branch_pattern in run_on_hg_branches:
        if re.match(expected_hg_branch_pattern, hg_branch):
            return True

    return False


def copy_attributes_from_dependent_job(dep_job, denylist=()):
    return {
        attr: dep_job.attributes[attr]
        for attr in _COPYABLE_ATTRIBUTES
        if attr in dep_job.attributes and attr not in denylist
    }


def sorted_unique_list(*args):
    """Join one or more lists, and return a sorted list of unique members"""
    combined = set().union(*args)
    return sorted(combined)


def release_level(project):
    """
    Whether this is a staging release or not.

    :return str: One of "production" or "staging".
    """
    return "production" if project in RELEASE_PROJECTS else "staging"


def is_try(params):
    """
    Determine whether this graph is being built on a try project or for
    `mach try fuzzy`.
    """
    return "try" in params["project"] or params["try_mode"] == "try_select"


def task_name(task):
    if task.label.startswith(task.kind + "-"):
        return task.label[len(task.kind) + 1 :]
    raise AttributeError(f"Task {task.label} does not have a name.")
