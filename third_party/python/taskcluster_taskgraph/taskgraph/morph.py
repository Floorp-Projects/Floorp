# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Graph morphs are modifications to task-graphs that take place *after* the
optimization phase.

These graph morphs are largely invisible to developers running `./mach`
locally, so they should be limited to changes that do not modify the meaning of
the graph.
"""

# Note that the translation of `{'task-reference': '..'}` and
# `artifact-reference` are handled in the optimization phase (since
# optimization involves dealing with taskIds directly).  Similarly,
# `{'relative-datestamp': '..'}` is handled at the last possible moment during
# task creation.


import logging
import os
import re

from slugid import nice as slugid

from .graph import Graph
from .task import Task
from .taskgraph import TaskGraph
from .util.workertypes import get_worker_type

here = os.path.abspath(os.path.dirname(__file__))
logger = logging.getLogger(__name__)
MAX_ROUTES = 10

registered_morphs = []


def register_morph(func):
    registered_morphs.append(func)


def amend_taskgraph(taskgraph, label_to_taskid, to_add):
    """Add the given tasks to the taskgraph, returning a new taskgraph"""
    new_tasks = taskgraph.tasks.copy()
    new_edges = set(taskgraph.graph.edges)
    for task in to_add:
        new_tasks[task.task_id] = task
        assert task.label not in label_to_taskid
        label_to_taskid[task.label] = task.task_id
        for depname, dep in task.dependencies.items():
            new_edges.add((task.task_id, dep, depname))

    taskgraph = TaskGraph(new_tasks, Graph(set(new_tasks), new_edges))
    return taskgraph, label_to_taskid


def derive_index_task(task, taskgraph, label_to_taskid, parameters, graph_config):
    """Create the shell of a task that depends on `task` and on the given docker
    image."""
    purpose = "index-task"
    label = f"{purpose}-{task.label}"
    provisioner_id, worker_type = get_worker_type(
        graph_config, "misc", parameters["level"]
    )

    task_def = {
        "provisionerId": provisioner_id,
        "workerType": worker_type,
        "dependencies": [task.task_id],
        "created": {"relative-datestamp": "0 seconds"},
        "deadline": task.task["deadline"],
        # no point existing past the parent task's deadline
        "expires": task.task["deadline"],
        "metadata": {
            "name": label,
            "description": "{} for {}".format(
                purpose, task.task["metadata"]["description"]
            ),
            "owner": task.task["metadata"]["owner"],
            "source": task.task["metadata"]["source"],
        },
        "scopes": [],
        "payload": {
            "image": {
                "path": "public/image.tar.zst",
                "namespace": "taskgraph.cache.level-3.docker-images.v2.index-task.latest",
                "type": "indexed-image",
            },
            "features": {
                "taskclusterProxy": True,
            },
            "maxRunTime": 600,
        },
    }

    # only include the docker-image dependency here if it is actually in the
    # taskgraph (has not been optimized).  It is included in
    # task_def['dependencies'] unconditionally.
    dependencies = {"parent": task.task_id}

    task = Task(
        kind="misc",
        label=label,
        attributes={},
        task=task_def,
        dependencies=dependencies,
    )
    task.task_id = slugid()
    return task, taskgraph, label_to_taskid


def make_index_task(parent_task, taskgraph, label_to_taskid, parameters, graph_config):
    index_paths = [
        r.split(".", 1)[1] for r in parent_task.task["routes"] if r.startswith("index.")
    ]
    parent_task.task["routes"] = [
        r for r in parent_task.task["routes"] if not r.startswith("index.")
    ]

    task, taskgraph, label_to_taskid = derive_index_task(
        parent_task, taskgraph, label_to_taskid, parameters, graph_config
    )

    # we need to "summarize" the scopes, otherwise a particularly
    # namespace-heavy index task might have more scopes than can fit in a
    # temporary credential.
    scopes = set()
    domain_index_regex = re.compile(
        r"({trust_domain}\.v2\.[^.]*\.).*".format(
            trust_domain=re.escape(graph_config["trust-domain"])
        )
    )
    index_path_res = [domain_index_regex]
    for path in graph_config["taskgraph"].get("index-path-regexes", ()):
        index_path_res.append(re.compile(path))
    for path in index_paths:
        for index_path_re in index_path_res:
            match = index_path_re.match(path)
            if match:
                path = match.group(1) + "*"
                break
        scope = f"index:insert-task:{path}"
        scopes.add(scope)
    task.task["scopes"] = sorted(scopes)

    task.task["payload"]["command"] = ["insert-indexes.js"] + index_paths
    task.task["payload"]["env"] = {
        "TARGET_TASKID": parent_task.task_id,
        "INDEX_RANK": parent_task.task.get("extra", {}).get("index", {}).get("rank", 0),
    }
    return task, taskgraph, label_to_taskid


@register_morph
def add_index_tasks(taskgraph, label_to_taskid, parameters, graph_config):
    """
    The TaskCluster queue only allows 10 routes on a task, but we have tasks
    with many more routes, for purposes of indexing. This graph morph adds
    "index tasks" that depend on such tasks and do the index insertions
    directly, avoiding the limits on task.routes.
    """
    logger.debug("Morphing: adding index tasks")

    added = []
    for label, task in taskgraph.tasks.items():
        if len(task.task.get("routes", [])) <= MAX_ROUTES:
            continue
        task, taskgraph, label_to_taskid = make_index_task(
            task, taskgraph, label_to_taskid, parameters, graph_config
        )
        added.append(task)

    if added:
        taskgraph, label_to_taskid = amend_taskgraph(taskgraph, label_to_taskid, added)
        logger.info(f"Added {len(added)} index tasks")

    return taskgraph, label_to_taskid


def _get_morph_url():
    """
    Guess a URL for the current file, for source metadata for created tasks.

    If we checked out the taskgraph code with run-task in the decision task,
    we can use TASKGRAPH_* to find the right version, which covers the
    existing use case.
    """
    taskgraph_repo = os.environ.get(
        "TASKGRAPH_HEAD_REPOSITORY", "https://github.com/taskcluster/taskgraph"
    )
    taskgraph_rev = os.environ.get("TASKGRAPH_HEAD_REV", "default")
    return f"{taskgraph_repo}/raw-file/{taskgraph_rev}/src/taskgraph/morph.py"


@register_morph
def add_code_review_task(taskgraph, label_to_taskid, parameters, graph_config):
    logger.debug("Morphing: adding code review task")

    review_config = parameters.get("code-review")
    if not review_config:
        return taskgraph, label_to_taskid

    code_review_tasks = {}
    for label, task in taskgraph.tasks.items():
        if task.attributes.get("code-review"):
            code_review_tasks[task.label] = task.task_id

    if code_review_tasks:
        code_review_task_def = {
            "provisionerId": "built-in",
            "workerType": "succeed",
            "dependencies": sorted(code_review_tasks.values()),
            # This option permits to run the task
            # regardless of the dependencies tasks exit status
            # as we are interested in the task failures
            "requires": "all-resolved",
            "created": {"relative-datestamp": "0 seconds"},
            "deadline": {"relative-datestamp": "1 day"},
            # no point existing past the parent task's deadline
            "expires": {"relative-datestamp": "1 day"},
            "metadata": {
                "name": "code-review",
                "description": "List all issues found in static analysis and linting tasks",
                "owner": parameters["owner"],
                "source": _get_morph_url(),
            },
            "scopes": [],
            "payload": {},
            "routes": ["project.relman.codereview.v1.try_ending"],
            "extra": {
                "code-review": {
                    "phabricator-build-target": review_config[
                        "phabricator-build-target"
                    ],
                    "repository": parameters["head_repository"],
                    "revision": parameters["head_rev"],
                }
            },
        }
        task = Task(
            kind="misc",
            label="code-review",
            attributes={},
            task=code_review_task_def,
            dependencies=code_review_tasks,
        )
        task.task_id = slugid()
        taskgraph, label_to_taskid = amend_taskgraph(taskgraph, label_to_taskid, [task])
        logger.info("Added code review task.")

    return taskgraph, label_to_taskid


def morph(taskgraph, label_to_taskid, parameters, graph_config):
    """Apply all morphs"""
    for m in registered_morphs:
        taskgraph, label_to_taskid = m(
            taskgraph, label_to_taskid, parameters, graph_config
        )
    return taskgraph, label_to_taskid
