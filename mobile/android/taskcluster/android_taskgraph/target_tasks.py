# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import re
from subprocess import CalledProcessError
from redo import retry

from mozilla_version.maven import MavenVersion
from taskgraph.target_tasks import _target_task, target_tasks_default
from taskgraph.util.taskcluster import find_task_id
from taskgraph.util.vcs import get_repository

from android_taskgraph.release_type import does_task_match_release_type


def index_exists(index_path, reason=""):
    print(f"Looking for existing index {index_path} {reason}...")
    try:
        task_id = find_task_id(index_path)
        print(f"Index {index_path} exists: taskId {task_id}")
        return True
    except KeyError:
        print(f"Index {index_path} doesn't exist.")
        return False


@_target_task("nightly")
def target_tasks_nightly(full_task_graph, parameters, graph_config):
    def filter(task, parameters):
        build_type = task.attributes.get("build-type", "")
        return build_type in ("nightly", "focus-nightly", "fenix-nightly")

    index_path = (
        f"{graph_config['trust-domain']}.v2.{parameters['project']}.branch."
        f"{parameters['head_ref']}.revision.{parameters['head_rev']}.taskgraph.decision-nightly"
    )
    if os.environ.get("MOZ_AUTOMATION") and retry(
        index_exists,
        args=(index_path,),
        kwargs={
            "reason": "to avoid triggering multiple nightlies off the same revision",
        },
    ):
        return []

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("nightly-test")
def target_tasks_nightly_test(full_task_graph, parameters, graph_config):
    def filter(task, parameters):
        return task.attributes.get("nightly-test", False)
    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("promote")
def target_tasks_promote(full_task_graph, parameters, graph_config):
    return _filter_release_promotion(
        full_task_graph, parameters, filtered_for_candidates=[], shipping_phase="promote"
    )


@_target_task("push")
def target_tasks_push(full_task_graph, parameters, graph_config):
    filtered_for_candidates = target_tasks_promote(
        full_task_graph,
        parameters,
        graph_config,
    )
    return _filter_release_promotion(
        full_task_graph, parameters, filtered_for_candidates, shipping_phase="push"
    )


@_target_task("ship")
def target_tasks_ship(full_task_graph, parameters, graph_config):
    filtered_for_candidates = target_tasks_push(
        full_task_graph,
        parameters,
        graph_config,
    )
    return _filter_release_promotion(
        full_task_graph, parameters, filtered_for_candidates, shipping_phase="ship"
    )


def _filter_release_promotion(full_task_graph, parameters, filtered_for_candidates, shipping_phase):
    def filter(task, parameters):
        # Include promotion tasks; these will be optimized out
        if task.label in filtered_for_candidates:
            return True

        if (
            task.attributes.get("shipping_phase") == shipping_phase
            and does_task_match_release_type(task, parameters["release_type"])
        ):
            return True

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("default")
def target_tasks_ac_default(full_task_graph, parameters, graph_config):
    def filter(task):
        # Trigger the nightly cron hook when the GV major version changes
        if task.kind != "trigger-nightly":
            return True
        repo = get_repository(os.getcwd())
        if get_gv_version(repo, parameters["base_rev"]) != get_gv_version(
            repo, parameters["head_rev"]
        ):
            return True
        return False

    return [
        l
        for l, t in full_task_graph.tasks.items()
        if l in target_tasks_default(full_task_graph, parameters, graph_config)
        and filter(t)
    ]


def get_gv_version(repo, revision):
    gecko_kt_path = get_gecko_kt_path(repo, revision)
    gecko_kt = repo.run(
        "show", f"{revision}:{gecko_kt_path}"
    )
    match = re.search(r'version = "([^"]*)"', gecko_kt, re.MULTILINE)
    if not match:
        raise Exception(f"Couldn't parse geckoview version on commit {revision}")
    return MavenVersion.parse(match.group(1)).major_number


GECKO_KT_MOVED_REVISION = "4335165c898b18d7c1b9ad1690f69ae07c5a5ba2"


def get_gecko_kt_path(repo, revision):
    try:
        # This command returns a different exit code depending on whether the former revision
        # is an ancestor of the latter
        #
        # https://git-scm.com/docs/git-merge-base#Documentation/git-merge-base.txt---is-ancestor
        repo.run(
            "merge-base", "--is-ancestor", GECKO_KT_MOVED_REVISION, revision
        )
        return "android-components/plugins/dependencies/src/main/java/Gecko.kt"
    except CalledProcessError:
        return "android-components/buildSrc/src/main/java/Gecko.kt"


@_target_task("screenshots")
def target_tasks_screnshots(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to generate screenshots on a real device."""

    def filter(task, parameters):
        return task.attributes.get("screenshots", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]


@_target_task("legacy_api_ui_tests")
def target_tasks_legacy_api_ui_tests(full_task_graph, parameters, graph_config):
    """Select the set of tasks required to run select UI tests on other API."""

    def filter(task, parameters):
        return task.attributes.get("legacy", False)

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]
