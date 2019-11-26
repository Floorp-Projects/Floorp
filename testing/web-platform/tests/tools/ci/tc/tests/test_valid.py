import json
import os

import jsone
import mock
import pytest
import requests
import sys
import yaml
from jsonschema import validate

here = os.path.dirname(__file__)
root = os.path.abspath(os.path.join(here, "..", "..", "..", ".."))


def data_path(filename):
    return os.path.join(here, "..", "testdata", filename)


@pytest.mark.xfail(sys.version_info.major == 2,
                   reason="taskcluster library has an encoding bug "
                   "https://github.com/taskcluster/json-e/issues/338")
def test_verify_taskcluster_yml():
    """Verify that the json-e in the .taskcluster.yml is valid"""
    with open(os.path.join(root, ".taskcluster.yml")) as f:
        template = yaml.safe_load(f)

    events = [("pr_event.json", "github-pull-request", "Pull Request"),
              ("master_push_event.json", "github-push", "Push to master")]

    for filename, tasks_for, title in events:
        with open(data_path(filename)) as f:
            event = json.load(f)

        context = {"tasks_for": tasks_for,
                   "event": event,
                   "as_slugid": lambda x: x}

        jsone.render(template, context)


def test_verify_payload():
    """Verify that the decision task produces tasks with a valid payload"""
    from tools.ci.tc.decision import decide

    create_task_schema = requests.get(
        "https://raw.githubusercontent.com/taskcluster/taskcluster/blob/master/services/queue/schemas/v1/create-task-request.yml")
    create_task_schema = yaml.safe_load(create_task_schema.content)

    payload_schema = requests.get("https://raw.githubusercontent.com/taskcluster/docker-worker/master/schemas/v1/payload.json").json()

    jobs = ["lint",
            "manifest_upload",
            "resources_unittest",
            "tools_unittest",
            "wpt_integration",
            "wptrunner_infrastructure",
            "wptrunner_unittest"]

    for filename in ["pr_event.json", "master_push_event.json"]:
        with open(data_path(filename)) as f:
            event = json.load(f)

        with mock.patch("tools.ci.tc.decision.get_fetch_rev", return_value=(event["after"], None)):
            with mock.patch("tools.ci.tc.decision.get_run_jobs", return_value=set(jobs)):
                task_id_map = decide(event)
        for name, (task_id, task_data) in task_id_map.items():
            try:
                validate(instance=task_data, schema=create_task_schema)
                validate(instance=task_data["payload"], schema=payload_schema)
            except Exception as e:
                print("Validation failed for task '%s':\n%s" % (name, json.dumps(task_data, indent=2)))
                raise e
