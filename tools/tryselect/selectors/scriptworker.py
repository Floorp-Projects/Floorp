# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys

import requests

from taskgraph.parameters import Parameters
from taskgraph.util.taskcluster import find_task_id, get_artifact, get_session
from taskgraph.util.taskgraph import find_existing_tasks

from ..cli import BaseTryParser
from ..push import push_to_try

TASK_TYPES = {
    "linux-signing": [
        "build-signing-linux-shippable/opt",
        "build-signing-linux64-shippable/opt",
        "build-signing-win64-shippable/opt",
        "build-signing-win32-shippable/opt",
        "repackage-signing-win64-shippable/opt",
        "repackage-signing-win32-shippable/opt",
        "repackage-signing-msi-win32-shippable/opt",
        "repackage-signing-msi-win64-shippable/opt",
        "mar-signing-linux64-shippable/opt",
        "partials-signing-linux64-shippable/opt",
    ],
    "mac-signing": ["build-signing-macosx64-shippable/opt"],
    "beetmover-candidates": ["beetmover-repackage-linux64-shippable/opt"],
    "bouncer-submit": ["release-bouncer-sub-firefox"],
    "balrog-submit": [
        "release-balrog-submit-toplevel-firefox",
        "balrog-linux64-shippable/opt",
    ],
    "tree": ["release-early-tagging-firefox", "release-version-bump-firefox"],
}

RELEASE_TO_BRANCH = {
    "beta": "releases/mozilla-beta",
    "release": "releases/mozilla-release",
}


class ScriptworkerParser(BaseTryParser):
    name = "scriptworker"
    arguments = [
        [
            ["task_type"],
            {
                "choices": ["list"] + list(TASK_TYPES.keys()),
                "metavar": "TASK-TYPE",
                "help": "Scriptworker task types to run. (Use `list` to show possibilities)",
            },
        ],
        [
            ["--release-type"],
            {
                "choices": ["nightly"] + list(RELEASE_TO_BRANCH.keys()),
                "default": "beta",
                "help": "Release type to run",
            },
        ],
    ]

    common_groups = ["push"]
    task_configs = ["worker-overrides"]


def get_releases(branch):
    response = requests.get(
        "https://shipitapi-public.services.mozilla.com/releases",
        params={"product": "firefox", "branch": branch, "status": "shipped"},
        headers={"Accept": ["application/json"]},
    )
    response.raise_for_status()
    return response.json()


def get_release_graph(release):
    for phase in release["phases"]:
        if phase["name"] in ("ship_firefox",):
            return phase["actionTaskId"]
    raise Exception("No ship phase.")


def get_nightly_graph():
    return find_task_id(
        "gecko.v2.mozilla-central.latest.taskgraph.decision-nightly-desktop"
    )


def print_available_task_types():
    print("Available task types:")
    for task_type, tasks in TASK_TYPES.items():
        print(" " * 4 + "{}:".format(task_type))
        for task in tasks:
            print(" " * 8 + "- {}".format(task))


def get_hg_file(parameters, path):
    session = get_session()
    response = session.get(parameters.file_url(path))
    response.raise_for_status()
    return response.content


def run(
    task_type,
    release_type,
    try_config=None,
    push=True,
    message="{msg}",
    closed_tree=False,
):
    if task_type == "list":
        print_available_task_types()
        sys.exit(0)

    if release_type == "nightly":
        previous_graph = get_nightly_graph()
    else:
        release = get_releases(RELEASE_TO_BRANCH[release_type])[-1]
        previous_graph = get_release_graph(release)
    existing_tasks = find_existing_tasks([previous_graph])

    previous_parameters = Parameters(
        strict=False, **get_artifact(previous_graph, "public/parameters.yml")
    )

    # Copy L10n configuration from the commit the release we are using was
    # based on. This *should* ensure that the chunking of L10n tasks is the
    # same between graphs.
    files_to_change = {
        path: get_hg_file(previous_parameters, path)
        for path in [
            "browser/locales/l10n-changesets.json",
            "browser/locales/shipped-locales",
        ]
    }

    try_config = try_config or {}
    task_config = {
        "version": 2,
        "parameters": {
            "existing_tasks": existing_tasks,
            "try_task_config": try_config,
            "try_mode": "try_task_config",
        },
    }
    for param in (
        "app_version",
        "build_number",
        "next_version",
        "release_history",
        "release_product",
        "release_type",
        "version",
    ):
        task_config["parameters"][param] = previous_parameters[param]

    try_config["tasks"] = TASK_TYPES[task_type]
    for label in try_config["tasks"]:
        if label in existing_tasks:
            del existing_tasks[label]

    msg = "scriptworker tests: {}".format(task_type)
    return push_to_try(
        "scriptworker",
        message.format(msg=msg),
        push=push,
        closed_tree=closed_tree,
        try_task_config=task_config,
        files_to_change=files_to_change,
    )
