# -*- coding: utf-8 -*-

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import logging
import requests

from .create import create_tasks
from .decision import write_artifact
from .optimize import optimize_task_graph
from .taskgraph import TaskGraph
from .util.taskcluster import get_artifact


logger = logging.getLogger(__name__)
TREEHERDER_URL = "https://treeherder.mozilla.org/api"

# We set this to 5 for now because this is what SETA sets the
# count to for every repository/job. If this is ever changed,
# we'll need to have an API added to Treeherder to let us query
# how far back we should look.
MAX_BACKFILL_RESULTSETS = 5


def add_tasks(decision_task_id, task_labels, prefix=''):
    """
    Run the add-tasks task.  This function implements `mach taskgraph add-tasks`,
    and is responsible for

     * creating taskgraph of tasks asked for in parameters with respect to
     a given gecko decision task and schedule these jobs.
    """
    # read in the full graph for reference
    full_task_json = get_artifact(decision_task_id, "public/full-task-graph.json")
    decision_params = get_artifact(decision_task_id, "public/parameters.yml")
    all_tasks, full_task_graph = TaskGraph.from_json(full_task_json)

    target_tasks = set(task_labels)
    target_graph = full_task_graph.graph.transitive_closure(target_tasks)
    target_task_graph = TaskGraph(
        {l: all_tasks[l] for l in target_graph.nodes},
        target_graph)

    existing_tasks = get_artifact(decision_task_id, "public/label-to-taskid.json")

    # We don't want to optimize target tasks since they have been requested by user
    # Hence we put `target_tasks under` `do_not_optimize`
    optimized_graph, label_to_taskid = optimize_task_graph(target_task_graph=target_task_graph,
                                                           params=decision_params,
                                                           do_not_optimize=target_tasks,
                                                           existing_tasks=existing_tasks)

    # write out the optimized task graph to describe what will actually happen,
    # and the map of labels to taskids
    write_artifact('{}task-graph.json'.format(prefix), optimized_graph.to_json())
    write_artifact('{}label-to-taskid.json'.format(prefix), label_to_taskid)
    # actually create the graph
    create_tasks(optimized_graph, label_to_taskid, decision_params)


def backfill(project, job_id):
    """
    Run the backfill task.  This function implements `mach taskgraph backfill-task`,
    and is responsible for

     * Scheduling backfill jobs from a given treeherder resultset backwards until either
     a successful job is found or `N` jobs have been scheduled.
    """
    s = requests.Session()
    s.headers.update({"User-Agent": "gecko-intree-backfill-task"})

    job = s.get(url="{}/project/{}/jobs/{}/".format(TREEHERDER_URL, project, job_id)).json()

    if job["build_system_type"] != "taskcluster":
        logger.warning("Invalid build system type! Must be a Taskcluster job. Aborting.")
        return

    filters = dict((k, job[k]) for k in ("build_platform_id", "platform_option", "job_type_id"))

    resultset_url = "{}/project/{}/resultset/".format(TREEHERDER_URL, project)
    params = {"id__lt": job["result_set_id"], "count": MAX_BACKFILL_RESULTSETS}
    results = s.get(url=resultset_url, params=params).json()["results"]
    resultsets = [resultset["id"] for resultset in results]

    for decision in load_decisions(s, project, resultsets, filters):
        add_tasks(decision, [job["job_type_name"]], '{}-'.format(decision))


def add_talos(decision_task_id, times=1):
    """
    Run the add-talos task.  This function implements `mach taskgraph add-talos`,
    and is responsible for

     * Adding all talos jobs to a push.
    """
    full_task_json = get_artifact(decision_task_id, "public/full-task-graph.json")
    task_labels = [
        label for label, task in full_task_json.iteritems()
        if "talos_try_name" in task['attributes']
    ]
    for time in xrange(times):
        add_tasks(decision_task_id, task_labels, '{}-'.format(time))


def load_decisions(s, project, resultsets, filters):
    """
    Given a project, a list of revisions, and a dict of filters, return
    a list of taskIds from decision tasks.
    """
    project_url = "{}/project/{}/jobs/".format(TREEHERDER_URL, project)
    decision_url = "{}/jobdetail/".format(TREEHERDER_URL)
    decisions = []
    decision_ids = []

    for resultset in resultsets:
        unfiltered = []
        offset = 0
        jobs_per_call = 250
        while True:
            params = {"push_id": resultset, "count": jobs_per_call, "offset": offset}
            results = s.get(url=project_url, params=params).json()["results"]
            unfiltered += results
            if (len(results) < jobs_per_call):
                break
            offset += jobs_per_call
        filtered = [j for j in unfiltered if all([j[k] == filters[k] for k in filters])]
        if filtered and all([j["result"] == "success" for j in filtered]):
            logger.info("Push found with all green jobs for this type. Continuing.")
            break
        decisions += [t for t in unfiltered if t["job_type_name"] == "Gecko Decision Task"]

    for decision in decisions:
        params = {"job_guid": decision["job_guid"]}
        details = s.get(url=decision_url, params=params).json()["results"]
        inspect = [detail["url"] for detail in details if detail["value"] == "Inspect Task"][0]

        # Pull out the taskId from the URL e.g.
        # oN1NErz_Rf2DZJ1hi7YVfA from tools.taskcluster.net/task-inspector/#oN1NErz_Rf2DZJ1hi7YVfA/
        decision_ids.append(inspect.partition('#')[-1].rpartition('/')[0])
    return decision_ids
