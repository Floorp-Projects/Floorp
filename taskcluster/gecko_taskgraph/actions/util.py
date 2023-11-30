# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import concurrent.futures as futures
import copy
import logging
import os
import re
from functools import reduce

import jsone
import requests
from requests.exceptions import HTTPError
from slugid import nice as slugid
from taskgraph import create
from taskgraph.optimize.base import optimize_task_graph
from taskgraph.taskgraph import TaskGraph
from taskgraph.util.taskcluster import (
    CONCURRENCY,
    find_task_id,
    get_artifact,
    get_session,
    get_task_definition,
    list_tasks,
    parse_time,
)

from gecko_taskgraph.decision import read_artifact, rename_artifact, write_artifact
from gecko_taskgraph.util.taskcluster import trigger_hook
from gecko_taskgraph.util.taskgraph import find_decision_task

logger = logging.getLogger(__name__)

INDEX_TMPL = "gecko.v2.{}.pushlog-id.{}.decision"
PUSHLOG_TMPL = "{}/json-pushes?version=2&startID={}&endID={}"


def _tags_within_context(tags, context=[]):
    """A context of [] means that it *only* applies to a task group"""
    return any(
        all(tag in tags and tags[tag] == tag_set[tag] for tag in tag_set.keys())
        for tag_set in context
    )


def _extract_applicable_action(actions_json, action_name, task_group_id, task_id):
    """Extract action that applies to the given task or task group.

    A task (as defined by its tags) is said to match a tag-set if its
    tags are a super-set of the tag-set. A tag-set is a set of key-value pairs.

    An action (as defined by its context) is said to be relevant for
    a given task, if the task's tags match one of the tag-sets given
    in the context property of the action.

    The order of the actions is significant. When multiple actions apply to a
    task the first one takes precedence.

    For more details visit:
    https://docs.taskcluster.net/docs/manual/design/conventions/actions/spec
    """
    if task_id:
        tags = get_task_definition(task_id).get("tags")

    for _action in actions_json["actions"]:
        if action_name != _action["name"]:
            continue

        context = _action.get("context", [])
        # Ensure the task is within the context of the action
        if task_id and tags and _tags_within_context(tags, context):
            return _action
        if context == []:
            return _action

    available_actions = ", ".join(sorted({a["name"] for a in actions_json["actions"]}))
    raise LookupError(
        "{} action is not available for this task. Available: {}".format(
            action_name, available_actions
        )
    )


def trigger_action(action_name, decision_task_id, task_id=None, input={}):
    if not decision_task_id:
        raise ValueError("No decision task. We can't find the actions artifact.")
    actions_json = get_artifact(decision_task_id, "public/actions.json")
    if actions_json["version"] != 1:
        raise RuntimeError("Wrong version of actions.json, unable to continue")

    # These values substitute $eval in the template
    context = {
        "input": input,
        "taskId": task_id,
        "taskGroupId": decision_task_id,
    }
    # https://docs.taskcluster.net/docs/manual/design/conventions/actions/spec#variables
    context.update(actions_json["variables"])
    action = _extract_applicable_action(
        actions_json, action_name, decision_task_id, task_id
    )
    kind = action["kind"]
    if create.testing:
        logger.info(f"Skipped triggering action for {kind} as testing is enabled")
    elif kind == "hook":
        hook_payload = jsone.render(action["hookPayload"], context)
        trigger_hook(action["hookGroupId"], action["hookId"], hook_payload)
    else:
        raise NotImplementedError(f"Unable to submit actions with {kind} kind.")


def get_pushes_from_params_input(parameters, input):
    inclusive_tweak = 1 if input.get("inclusive") else 0
    return get_pushes(
        project=parameters["head_repository"],
        end_id=int(parameters["pushlog_id"]) - (1 - inclusive_tweak),
        depth=input.get("depth", 9) + inclusive_tweak,
    )


def get_pushes(project, end_id, depth, full_response=False):
    pushes = []
    while True:
        start_id = max(end_id - depth, 0)
        pushlog_url = PUSHLOG_TMPL.format(project, start_id, end_id)
        logger.debug(pushlog_url)
        r = requests.get(pushlog_url)
        r.raise_for_status()
        pushes = pushes + list(r.json()["pushes"].keys())
        if len(pushes) >= depth:
            break

        end_id = start_id - 1
        start_id -= depth
        if start_id < 0:
            break

    pushes = sorted(pushes)[-depth:]
    push_dict = {push: r.json()["pushes"][push] for push in pushes}
    return push_dict if full_response else pushes


def get_decision_task_id(project, push_id):
    return find_task_id(INDEX_TMPL.format(project, push_id))


def get_parameters(decision_task_id):
    return get_artifact(decision_task_id, "public/parameters.yml")


def get_tasks_with_downstream(labels, full_task_graph, label_to_taskid):
    # Used to gather tasks when downstream tasks need to run as well
    return full_task_graph.graph.transitive_closure(
        set(labels), reverse=True
    ).nodes & set(label_to_taskid.keys())


def fetch_graph_and_labels(parameters, graph_config):
    decision_task_id = find_decision_task(parameters, graph_config)

    # First grab the graph and labels generated during the initial decision task
    full_task_graph = get_artifact(decision_task_id, "public/full-task-graph.json")
    logger.info("Load taskgraph from JSON.")
    _, full_task_graph = TaskGraph.from_json(full_task_graph)
    label_to_taskid = get_artifact(decision_task_id, "public/label-to-taskid.json")
    label_to_taskids = {label: [task_id] for label, task_id in label_to_taskid.items()}

    logger.info("Fetching additional tasks from action and cron tasks.")
    # fetch everything in parallel; this avoids serializing any delay in downloading
    # each artifact (such as waiting for the artifact to be mirrored locally)
    with futures.ThreadPoolExecutor(CONCURRENCY) as e:
        fetches = []

        # fetch any modifications made by action tasks and add the new tasks
        def fetch_action(task_id):
            logger.info(f"fetching label-to-taskid.json for action task {task_id}")
            try:
                run_label_to_id = get_artifact(task_id, "public/label-to-taskid.json")
                label_to_taskid.update(run_label_to_id)
                for label, task_id in run_label_to_id.items():
                    label_to_taskids.setdefault(label, []).append(task_id)
            except HTTPError as e:
                if e.response.status_code != 404:
                    raise
                logger.debug(f"No label-to-taskid.json found for {task_id}: {e}")

        head_rev_param = "{}head_rev".format(graph_config["project-repo-param-prefix"])

        namespace = "{}.v2.{}.revision.{}.taskgraph.actions".format(
            graph_config["trust-domain"],
            parameters["project"],
            parameters[head_rev_param],
        )
        for task_id in list_tasks(namespace):
            fetches.append(e.submit(fetch_action, task_id))

        # Similarly for cron tasks..
        def fetch_cron(task_id):
            logger.info(f"fetching label-to-taskid.json for cron task {task_id}")
            try:
                run_label_to_id = get_artifact(task_id, "public/label-to-taskid.json")
                label_to_taskid.update(run_label_to_id)
                for label, task_id in run_label_to_id.items():
                    label_to_taskids.setdefault(label, []).append(task_id)
            except HTTPError as e:
                if e.response.status_code != 404:
                    raise
                logger.debug(f"No label-to-taskid.json found for {task_id}: {e}")

        namespace = "{}.v2.{}.revision.{}.cron".format(
            graph_config["trust-domain"],
            parameters["project"],
            parameters[head_rev_param],
        )
        for task_id in list_tasks(namespace):
            fetches.append(e.submit(fetch_cron, task_id))

        # now wait for each fetch to complete, raising an exception if there
        # were any issues
        for f in futures.as_completed(fetches):
            f.result()

    return (decision_task_id, full_task_graph, label_to_taskid, label_to_taskids)


def create_task_from_def(task_def, level, action_tag=None):
    """Create a new task from a definition rather than from a label
    that is already in the full-task-graph. The task definition will
    have {relative-datestamp': '..'} rendered just like in a decision task.
    Use this for entirely new tasks or ones that change internals of the task.
    It is useful if you want to "edit" the full_task_graph and then hand
    it to this function. No dependencies will be scheduled. You must handle
    this yourself. Seeing how create_tasks handles it might prove helpful."""
    task_def["schedulerId"] = f"gecko-level-{level}"
    label = task_def["metadata"]["name"]
    task_id = slugid()
    session = get_session()
    if action_tag:
        task_def.setdefault("tags", {}).setdefault("action", action_tag)
    create.create_task(session, task_id, label, task_def)


def update_parent(task, graph):
    task.task.setdefault("extra", {})["parent"] = os.environ.get("TASK_ID", "")
    return task


def update_action_tag(task, graph, action_tag):
    task.task.setdefault("tags", {}).setdefault("action", action_tag)
    return task


def update_dependencies(task, graph):
    if os.environ.get("TASK_ID"):
        task.task.setdefault("dependencies", []).append(os.environ["TASK_ID"])
    return task


def create_tasks(
    graph_config,
    to_run,
    full_task_graph,
    label_to_taskid,
    params,
    decision_task_id,
    suffix="",
    modifier=lambda t: t,
    action_tag=None,
):
    """Create new tasks.  The task definition will have {relative-datestamp':
    '..'} rendered just like in a decision task.  Action callbacks should use
    this function to create new tasks,
    allowing easy debugging with `mach taskgraph action-callback --test`.
    This builds up all required tasks to run in order to run the tasks requested.

    Optionally this function takes a `modifier` function that is passed in each
    task before it is put into a new graph. It should return a valid task. Note
    that this is passed _all_ tasks in the graph, not just the set in to_run. You
    may want to skip modifying tasks not in your to_run list.

    If `suffix` is given, then it is used to give unique names to the resulting
    artifacts.  If you call this function multiple times in the same action,
    pass a different suffix each time to avoid overwriting artifacts.

    If you wish to create the tasks in a new group, leave out decision_task_id.

    Returns an updated label_to_taskid containing the new tasks"""
    import gecko_taskgraph.optimize  # noqa: triggers registration of strategies

    if suffix != "":
        suffix = f"-{suffix}"
    to_run = set(to_run)

    #  Copy to avoid side-effects later
    full_task_graph = copy.deepcopy(full_task_graph)
    label_to_taskid = label_to_taskid.copy()

    target_graph = full_task_graph.graph.transitive_closure(to_run)
    target_task_graph = TaskGraph(
        {l: modifier(full_task_graph[l]) for l in target_graph.nodes}, target_graph
    )
    target_task_graph.for_each_task(update_parent)
    if action_tag:
        target_task_graph.for_each_task(update_action_tag, action_tag)
    if decision_task_id and decision_task_id != os.environ.get("TASK_ID"):
        target_task_graph.for_each_task(update_dependencies)
    optimized_task_graph, label_to_taskid = optimize_task_graph(
        target_task_graph,
        to_run,
        params,
        to_run,
        decision_task_id,
        existing_tasks=label_to_taskid,
    )
    write_artifact(f"task-graph{suffix}.json", optimized_task_graph.to_json())
    write_artifact(f"label-to-taskid{suffix}.json", label_to_taskid)
    write_artifact(f"to-run{suffix}.json", list(to_run))
    create.create_tasks(
        graph_config,
        optimized_task_graph,
        label_to_taskid,
        params,
        decision_task_id,
    )
    return label_to_taskid


def _update_reducer(accumulator, new_value):
    "similar to set or dict `update` method, but returning the modified object"
    accumulator.update(new_value)
    return accumulator


def combine_task_graph_files(suffixes):
    """Combine task-graph-{suffix}.json files into a single task-graph.json file.

    Since Chain of Trust verification requires a task-graph.json file that
    contains all children tasks, we can combine the various task-graph-0.json
    type files into a master task-graph.json file at the end.

    Actions also look for various artifacts, so we combine those in a similar
    fashion.

    In the case where there is only one suffix, we simply rename it to avoid the
    additional cost of uploading two copies of the same data.
    """

    if len(suffixes) == 1:
        for filename in ["task-graph", "label-to-taskid", "to-run"]:
            rename_artifact(f"{filename}-{suffixes[0]}.json", f"{filename}.json")
        return

    def combine(file_contents, base):
        return reduce(_update_reducer, file_contents, base)

    files = [read_artifact(f"task-graph-{suffix}.json") for suffix in suffixes]
    write_artifact("task-graph.json", combine(files, dict()))

    files = [read_artifact(f"label-to-taskid-{suffix}.json") for suffix in suffixes]
    write_artifact("label-to-taskid.json", combine(files, dict()))

    files = [read_artifact(f"to-run-{suffix}.json") for suffix in suffixes]
    write_artifact("to-run.json", list(combine(files, set())))


def relativize_datestamps(task_def):
    """
    Given a task definition as received from the queue, convert all datestamps
    to {relative_datestamp: ..} format, with the task creation time as "now".
    The result is useful for handing to ``create_task``.
    """
    base = parse_time(task_def["created"])
    # borrowed from https://github.com/epoberezkin/ajv/blob/master/lib/compile/formats.js
    ts_pattern = re.compile(
        r"^\d\d\d\d-[0-1]\d-[0-3]\d[t\s]"
        r"(?:[0-2]\d:[0-5]\d:[0-5]\d|23:59:60)(?:\.\d+)?"
        r"(?:z|[+-]\d\d:\d\d)$",
        re.I,
    )

    def recurse(value):
        if isinstance(value, str):
            if ts_pattern.match(value):
                value = parse_time(value)
                diff = value - base
                return {"relative-datestamp": f"{int(diff.total_seconds())} seconds"}
        if isinstance(value, list):
            return [recurse(e) for e in value]
        if isinstance(value, dict):
            return {k: recurse(v) for k, v in value.items()}
        return value

    return recurse(task_def)


def add_args_to_command(cmd_parts, extra_args=[]):
    """
    Add custom command line args to a given command.
    args:
      cmd_parts: the raw command as seen by taskcluster
      extra_args: array of args we want to add
    """
    # Prevent modification of the caller's copy of cmd_parts
    cmd_parts = copy.deepcopy(cmd_parts)
    cmd_type = "default"
    if len(cmd_parts) == 1 and isinstance(cmd_parts[0], dict):
        # windows has single cmd part as dict: 'task-reference', with long string
        cmd_parts = cmd_parts[0]["task-reference"].split(" ")
        cmd_type = "dict"
    elif len(cmd_parts) == 1 and isinstance(cmd_parts[0], str):
        # windows has single cmd part as a long string
        cmd_parts = cmd_parts[0].split(" ")
        cmd_type = "unicode"
    elif len(cmd_parts) == 1 and isinstance(cmd_parts[0], list):
        # osx has an single value array with an array inside
        cmd_parts = cmd_parts[0]
        cmd_type = "subarray"
    elif len(cmd_parts) == 2 and isinstance(cmd_parts[1], list):
        # osx has an double value array with an array inside each element.
        # The first element is a pre-requisite command while the second
        # is the actual test command.
        cmd_type = "subarray2"

    if cmd_type == "subarray2":
        cmd_parts[1].extend(extra_args)
    else:
        cmd_parts.extend(extra_args)

    if cmd_type == "dict":
        cmd_parts = [{"task-reference": " ".join(cmd_parts)}]
    elif cmd_type == "unicode":
        cmd_parts = [" ".join(cmd_parts)]
    elif cmd_type == "subarray":
        cmd_parts = [cmd_parts]
    return cmd_parts
