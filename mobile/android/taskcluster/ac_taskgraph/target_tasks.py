# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
from redo import retry

from taskgraph.target_tasks import _target_task
from taskgraph.util.taskcluster import find_task_id


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
        return task.attributes.get("build-type", "") == "nightly"

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


@_target_task("release")
def target_tasks_release(full_task_graph, parameters, graph_config):
    def filter(task, parameters):
        # Mark-as-shipped is always red on github-release and it confuses people.
        # This task cannot be green if we kick off a release through github-releases, so
        # let's exlude that task there.
        if (
            task.kind == "mark-as-shipped"
            and parameters["tasks_for"] == "github-release"
        ):
            return False

        return task.attributes.get("build-type", "") == "release"

    return [l for l, t in full_task_graph.tasks.items() if filter(t, parameters)]
