# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import json
import logging

from taskgraph.util.parameterization import resolve_task_references
from taskgraph.util.taskcluster import get_artifact, get_task_definition, list_artifacts

from gecko_taskgraph.util.copy_task import copy_task

from .registry import register_callback_action
from .util import add_args_to_command, create_task_from_def, fetch_graph_and_labels

logger = logging.getLogger(__name__)


def get_failures(task_id, task_definition):
    """Returns a dict containing properties containing a list of
    directories containing test failures and a separate list of
    individual test failures from the errorsummary.log artifact for
    the task.

    Find test path to pass to the task in
    MOZHARNESS_TEST_PATHS.  If no appropriate test path can be
    determined, nothing is returned.
    """

    def fix_wpt_name(test):
        tests = [test]
        # TODO: find other cases to handle
        if ".any." in test:
            tests = ["%s.any.js" % test.split(".any.")[0]]
        if ".window.html" in test:
            tests = [test.replace(".window.html", ".window.js")]
        return tests

    # collect dirs that don't have a specific manifest
    dirs = set()
    tests = set()
    artifacts = list_artifacts(task_id)
    for artifact in artifacts:
        if "name" not in artifact or not artifact["name"].endswith("errorsummary.log"):
            continue

        stream = get_artifact(task_id, artifact["name"])
        if not stream:
            continue

        # We handle the stream as raw bytes because it may contain invalid
        # UTF-8 characters in portions other than those containing the error
        # messages we're looking for.
        for line in stream.read().split(b"\n"):
            if not line.strip():
                continue

            l = json.loads(line)
            if "group_results" in l.keys() and l["status"] != "OK":
                dirs.add(l["group_results"].group())

            elif "test" in l.keys():
                if not l["test"]:
                    print("Warning: no testname in errorsummary line: %s" % l)
                    continue
                if "signature" in l.keys():
                    # dealing with a crash
                    test_path = l["test"].split(" ")[0]

                    # tests with url params (wpt), will get confused here
                    if "?" not in test_path:
                        test_path = test_path.split(":")[-1]

                    # edge case where a crash on shutdown has a "test" name == group name
                    if test_path.endswith(".ini") or test_path.endswith(".list"):
                        continue

                    # edge cases with missing test names
                    if (
                        test_path is None
                        or test_path == "None"
                        or "SimpleTest" in test_path
                    ):
                        continue

                    if "web-platform" in task_definition["extra"]["suite"]:
                        test_path = fix_wpt_name(test_path)
                    else:
                        test_path = [test_path]

                    if test_path:
                        tests.update(test_path)
                else:
                    test_path = l["test"]

                    # tests with url params (wpt), will get confused here
                    if "?" not in test_path:
                        test_path = test_path.split(":")[-1]

                    if "==" in test_path or "!=" in test_path:
                        test_path = test_path.split(" ")[0]

                    # edge case where a crash on shutdown has a "test" name == group name
                    if test_path.endswith(".ini") or test_path.endswith(".list"):
                        continue

                    # edge cases with missing test names
                    if (
                        test_path is None
                        or test_path == "None"
                        or "SimpleTest" in test_path
                    ):
                        continue

                    if "status" not in l and "expected" not in l:
                        continue

                    if l["status"] != l["expected"]:
                        if l["status"] not in l.get("known_intermittent", []):
                            if "web-platform" in task_definition["extra"]["suite"]:
                                test_path = fix_wpt_name(test_path)
                            else:
                                test_path = [test_path]

                            if test_path:
                                tests.update(test_path)

                # only run the failing test not both test + dir
                if l["group"] in dirs:
                    dirs.remove(l["group"])

            # TODO: 10 is too much; how to get only NEW failures?
            if len(tests) > 10:
                break

        # turn group into dir by stripping off leafname
        dirs = set([d.split("/")[0:-1] for d in dirs])

    return {"dirs": sorted(dirs), "tests": sorted(tests)}


def create_confirm_failure_tasks(task_definition, failures, level):
    """
    Create tasks to re-run the original task plus tasks to test
    each failing test directory and individual path.

    """
    logger.info(f"Confirm Failures task:\n{json.dumps(task_definition, indent=2)}")

    # Operate on a copy of the original task_definition
    task_definition = copy_task(task_definition)

    task_name = task_definition["metadata"]["name"]
    repeatable_task = False
    if (
        "crashtest" in task_name
        or "mochitest" in task_name
        or "reftest" in task_name
        or "xpcshell" in task_name
        or "web-platform" in task_name
        and "jsreftest" not in task_name
    ):
        repeatable_task = True

    th_dict = task_definition["extra"]["treeherder"]
    symbol = th_dict["symbol"]

    suite = task_definition["extra"]["suite"]
    if "-coverage" in suite:
        suite = suite[: suite.index("-coverage")]
    is_wpt = "web-platform" in suite

    # command is a copy of task_definition['payload']['command'] from the original task.
    # It is used to create the new version containing the
    # task_definition['payload']['command'] with repeat_args which is updated every time
    # through the failure_group loop.

    command = copy_task(task_definition["payload"]["command"])

    th_dict["groupSymbol"] = th_dict["groupSymbol"] + "-cf"
    th_dict["tier"] = 3

    if repeatable_task:
        task_definition["payload"]["maxRunTime"] = 3600 * 3

    for failure_group in failures:
        if len(failures[failure_group]) == 0:
            continue
        if failure_group == "dirs":
            failure_group_suffix = "-d"
            # execute 5 total loops
            repeat_args = ["--repeat=4"] if repeatable_task else []
        elif failure_group == "tests":
            failure_group_suffix = "-t"
            # execute 10 total loops
            repeat_args = ["--repeat=9"] if repeatable_task else []
        else:
            logger.error(
                "create_confirm_failure_tasks: Unknown failure_group {}".format(
                    failure_group
                )
            )
            continue

        if repeat_args:
            task_definition["payload"]["command"] = add_args_to_command(
                command, extra_args=repeat_args
            )
        else:
            task_definition["payload"]["command"] = command

        for failure_path in failures[failure_group]:
            th_dict["symbol"] = symbol + failure_group_suffix
            fpath = failure_path.replace("\\", "/")
            if is_wpt:
                if fpath.startswith("/_mozilla"):
                    fpath = (
                        "testing/web-platform/mozilla/tests"
                        + fpath.split("_mozilla")[1]
                    )
                else:
                    fpath = "testing/web-platform/tests" + fpath
                # some wpt tests have params, those are not supported
                fpath = fpath.split("?")[0]
            task_definition["payload"]["env"]["MOZHARNESS_TEST_PATHS"] = json.dumps(
                {suite: [fpath]}, sort_keys=True
            )

            logger.info(
                "Creating task for path {} with command {}".format(
                    fpath, task_definition["payload"]["command"]
                )
            )
            create_task_from_def(task_definition, level)


@register_callback_action(
    name="confirm-failures",
    title="Confirm failures in job",
    symbol="cf",
    description="Re-run Tests for original manifest, directories or tests for failing tests.",
    order=150,
    context=[{"kind": "test"}],
    schema={
        "type": "object",
        "properties": {},
        "additionalProperties": False,
    },
)
def confirm_failures(parameters, graph_config, input, task_group_id, task_id):
    task = get_task_definition(task_id)
    decision_task_id, full_task_graph, label_to_taskid = fetch_graph_and_labels(
        parameters, graph_config
    )

    pre_task = full_task_graph.tasks[task["metadata"]["name"]]

    # fix up the task's dependencies, similar to how optimization would
    # have done in the decision
    dependencies = {
        name: label_to_taskid[label] for name, label in pre_task.dependencies.items()
    }

    task_definition = resolve_task_references(
        pre_task.label, pre_task.task, task_id, decision_task_id, dependencies
    )
    task_definition.setdefault("dependencies", []).extend(dependencies.values())

    failures = get_failures(task_id, task_definition)
    logger.info("confirm_failures: %s" % failures)
    create_confirm_failure_tasks(task_definition, failures, parameters["level"])
