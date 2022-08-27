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


import copy
import logging
import os
import re


from slugid import nice as slugid
from taskgraph.graph import Graph
from taskgraph.task import Task
from taskgraph.taskgraph import TaskGraph

from .util.workertypes import get_worker_type

here = os.path.abspath(os.path.dirname(__file__))
logger = logging.getLogger(__name__)
MAX_ROUTES = 10


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


def derive_misc_task(
    target_task,
    purpose,
    image,
    taskgraph,
    label_to_taskid,
    parameters,
    graph_config,
    dependencies,
):
    """Create the shell of a task that depends on `dependencies` and on the given docker
    image."""
    label = f"{purpose}-{target_task.label}"

    # this is why all docker image tasks are included in the target task graph: we
    # need to find them in label_to_taskid, even if nothing else required them
    image_taskid = label_to_taskid["docker-image-" + image]

    provisioner_id, worker_type = get_worker_type(
        graph_config,
        parameters,
        "misc",
    )

    deps = copy.copy(dependencies)
    deps["docker-image"] = image_taskid

    task_def = {
        "provisionerId": provisioner_id,
        "workerType": worker_type,
        "dependencies": [d for d in deps.values()],
        "created": {"relative-datestamp": "0 seconds"},
        "deadline": target_task.task["deadline"],
        # no point existing past the parent task's deadline
        "expires": target_task.task["deadline"],
        "metadata": {
            "name": label,
            "description": f"{purpose} for {target_task.description}",
            "owner": target_task.task["metadata"]["owner"],
            "source": target_task.task["metadata"]["source"],
        },
        "scopes": [],
        "payload": {
            "image": {
                "path": "public/image.tar.zst",
                "taskId": image_taskid,
                "type": "task-image",
            },
            "features": {"taskclusterProxy": True},
            "maxRunTime": 600,
        },
    }

    if image_taskid not in taskgraph.tasks:
        # The task above depends on the replaced docker-image not one in
        # this current graph.
        del deps["docker-image"]

    task = Task(
        kind="misc",
        label=label,
        attributes={},
        task=task_def,
        dependencies=deps,
    )
    task.task_id = slugid()
    return task


# these regular expressions capture route prefixes for which we have a star
# scope, allowing them to be summarized.  Each should correspond to a star scope
# in each Gecko `assume:repo:hg.mozilla.org/...` role.
SCOPE_SUMMARY_REGEXPS = [
    re.compile(r"(index:insert-task:docker\.images\.v1\.[^.]*\.).*"),
    re.compile(r"(index:insert-task:gecko\.v2\.[^.]*\.).*"),
    re.compile(r"(index:insert-task:comm\.v2\.[^.]*\.).*"),
]


def make_index_task(
    parent_task,
    taskgraph,
    label_to_taskid,
    parameters,
    graph_config,
    index_paths,
    index_rank,
    purpose,
    dependencies,
):
    task = derive_misc_task(
        parent_task,
        purpose,
        "index-task",
        taskgraph,
        label_to_taskid,
        parameters,
        graph_config,
        dependencies,
    )

    # we need to "summarize" the scopes, otherwise a particularly
    # namespace-heavy index task might have more scopes than can fit in a
    # temporary credential.
    scopes = set()
    for path in index_paths:
        scope = f"index:insert-task:{path}"
        for summ_re in SCOPE_SUMMARY_REGEXPS:
            match = summ_re.match(scope)
            if match:
                scope = match.group(1) + "*"
                break
        scopes.add(scope)
    task.task["scopes"] = sorted(scopes)

    task.task["payload"]["command"] = ["insert-indexes.js"] + index_paths
    task.task["payload"]["env"] = {
        "TARGET_TASKID": parent_task.task_id,
        "INDEX_RANK": index_rank,
    }
    return task


def add_index_tasks(
    taskgraph, label_to_taskid, parameters, graph_config, decision_task_id
):
    """
    The TaskCluster queue only allows 10 routes on a task, but we have tasks
    with many more routes, for purposes of indexing. This graph morph adds
    "index tasks" that depend on such tasks and do the index insertions
    directly, avoiding the limits on task.routes.
    """
    logger.debug("Morphing: adding index tasks")

    # Add indexes for tasks that exceed MAX_ROUTES.
    added = []
    for label, task in taskgraph.tasks.items():
        if len(task.task.get("routes", [])) <= MAX_ROUTES:
            continue
        index_paths = [
            r.split(".", 1)[1] for r in task.task["routes"] if r.startswith("index.")
        ]
        task.task["routes"] = [
            r for r in task.task["routes"] if not r.startswith("index.")
        ]
        added.append(
            make_index_task(
                task,
                taskgraph,
                label_to_taskid,
                parameters,
                graph_config,
                index_paths=index_paths,
                index_rank=task.task.get("extra", {}).get("index", {}).get("rank", 0),
                purpose="index-task",
                dependencies={"parent": task.task_id},
            )
        )

    if added:
        taskgraph, label_to_taskid = amend_taskgraph(taskgraph, label_to_taskid, added)
        logger.info(f"Added {len(added)} index tasks")

    return taskgraph, label_to_taskid


def add_eager_cache_index_tasks(
    taskgraph, label_to_taskid, parameters, graph_config, decision_task_id
):
    """
    Some tasks (e.g. cached tasks) we want to exist in the index before they even
    run/complete. Our current use is to allow us to depend on an unfinished cached
    task in future pushes. This graph morph adds "eager-index tasks" that depend on
    the decision task and do the index insertions directly, which does not need to
    wait on the pointed at task to complete.
    """
    logger.debug("Morphing: Adding eager cached index's")

    added = []
    for label, task in taskgraph.tasks.items():
        if "eager_indexes" not in task.attributes:
            continue
        eager_indexes = task.attributes["eager_indexes"]
        added.append(
            make_index_task(
                task,
                taskgraph,
                label_to_taskid,
                parameters,
                graph_config,
                index_paths=eager_indexes,
                index_rank=0,  # Be sure complete tasks get priority
                purpose="eager-index",
                dependencies={},
            )
        )

    if added:
        taskgraph, label_to_taskid = amend_taskgraph(taskgraph, label_to_taskid, added)
        logger.info(f"Added {len(added)} eager index tasks")
    return taskgraph, label_to_taskid


def add_try_task_duplicates(
    taskgraph, label_to_taskid, parameters, graph_config, decision_task_id
):
    try_config = parameters["try_task_config"]
    rebuild = try_config.get("rebuild")
    if rebuild:
        for task in taskgraph.tasks.values():
            if task.label in try_config.get("tasks", []):
                task.attributes["task_duplicates"] = rebuild
    return taskgraph, label_to_taskid


def morph(taskgraph, label_to_taskid, parameters, graph_config, decision_task_id):
    """Apply all morphs"""
    morphs = [
        add_eager_cache_index_tasks,
        add_index_tasks,
        add_try_task_duplicates,
    ]

    for m in morphs:
        taskgraph, label_to_taskid = m(
            taskgraph, label_to_taskid, parameters, graph_config, decision_task_id
        )
    return taskgraph, label_to_taskid
