# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import logging
import sys
from concurrent import futures

from slugid import nice as slugid

from taskgraph.util.parameterization import resolve_timestamps
from taskgraph.util.taskcluster import CONCURRENCY, get_session
from taskgraph.util.time import current_json_time

logger = logging.getLogger(__name__)

# this is set to true for `mach taskgraph action-callback --test`
testing = False


def create_tasks(graph_config, taskgraph, label_to_taskid, params, decision_task_id):
    taskid_to_label = {t: l for l, t in label_to_taskid.items()}

    # when running as an actual decision task, we use the decision task's
    # taskId as the taskGroupId.  The process that created the decision task
    # helpfully placed it in this same taskGroup.  If there is no $TASK_ID,
    # fall back to a slugid
    scheduler_id = "{}-level-{}".format(graph_config["trust-domain"], params["level"])

    # Add the taskGroupId, schedulerId and optionally the decision task
    # dependency
    for task_id in taskgraph.graph.nodes:
        task_def = taskgraph.tasks[task_id].task

        # if this task has no dependencies *within* this taskgraph, make it
        # depend on this decision task. If it has another dependency within
        # the taskgraph, then it already implicitly depends on the decision
        # task.  The result is that tasks do not start immediately. if this
        # loop fails halfway through, none of the already-created tasks run.
        if not any(t in taskgraph.tasks for t in task_def.get("dependencies", [])):
            task_def.setdefault("dependencies", []).append(decision_task_id)

        task_def["taskGroupId"] = decision_task_id
        task_def["schedulerId"] = scheduler_id

    # If `testing` is True, then run without parallelization
    concurrency = CONCURRENCY if not testing else 1
    session = get_session()
    with futures.ThreadPoolExecutor(concurrency) as e:
        fs = {}

        # We can't submit a task until its dependencies have been submitted.
        # So our strategy is to walk the graph and submit tasks once all
        # their dependencies have been submitted.
        tasklist = set(taskgraph.graph.visit_postorder())
        alltasks = tasklist.copy()

        def schedule_tasks():
            # bail out early if any futures have failed
            if any(f.done() and f.exception() for f in fs.values()):
                return

            to_remove = set()
            new = set()

            def submit(task_id, label, task_def):
                fut = e.submit(create_task, session, task_id, label, task_def)
                new.add(fut)
                fs[task_id] = fut

            for task_id in tasklist:
                task_def = taskgraph.tasks[task_id].task
                # If we haven't finished submitting all our dependencies yet,
                # come back to this later.
                # Some dependencies aren't in our graph, so make sure to filter
                # those out
                deps = set(task_def.get("dependencies", [])) & alltasks
                if any((d not in fs or not fs[d].done()) for d in deps):
                    continue

                submit(task_id, taskid_to_label[task_id], task_def)
                to_remove.add(task_id)

                # Schedule tasks as many times as task_duplicates indicates
                attributes = taskgraph.tasks[task_id].attributes
                for i in range(1, attributes.get("task_duplicates", 1)):
                    # We use slugid() since we want a distinct task id
                    submit(slugid(), taskid_to_label[task_id], task_def)
            tasklist.difference_update(to_remove)

            # as each of those futures complete, try to schedule more tasks
            for f in futures.as_completed(new):
                schedule_tasks()

        # start scheduling tasks and run until everything is scheduled
        schedule_tasks()

        # check the result of each future, raising an exception if it failed
        for f in futures.as_completed(fs.values()):
            f.result()


def create_task(session, task_id, label, task_def):
    # create the task using 'http://taskcluster/queue', which is proxied to the queue service
    # with credentials appropriate to this job.

    # Resolve timestamps
    now = current_json_time(datetime_format=True)
    task_def = resolve_timestamps(now, task_def)

    if testing:
        json.dump(
            [task_id, task_def],
            sys.stdout,
            sort_keys=True,
            indent=4,
            separators=(",", ": "),
        )
        # add a newline
        print("")
        return

    logger.info(f"Creating task with taskId {task_id} for {label}")
    res = session.put(f"http://taskcluster/queue/v1/task/{task_id}", json=task_def)
    if res.status_code != 200:
        try:
            logger.error(res.json()["message"])
        except Exception:
            logger.error(res.text)
        res.raise_for_status()
