import json
import os

import jsone
import mock
import pytest
import requests
import sys
import yaml
from jsonschema import validate

from tools.ci.tc import decision

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


@pytest.mark.parametrize("event_path,is_pr,files_changed,expected", [
    ("master_push_event.json", False, None,
     {'download-firefox-nightly',
      'wpt-firefox-nightly-testharness-chunk-1',
      'wpt-firefox-nightly-testharness-chunk-2',
      'wpt-firefox-nightly-testharness-chunk-3',
      'wpt-firefox-nightly-testharness-chunk-4',
      'wpt-firefox-nightly-testharness-chunk-5',
      'wpt-firefox-nightly-testharness-chunk-6',
      'wpt-firefox-nightly-testharness-chunk-7',
      'wpt-firefox-nightly-testharness-chunk-8',
      'wpt-firefox-nightly-testharness-chunk-9',
      'wpt-firefox-nightly-testharness-chunk-10',
      'wpt-firefox-nightly-testharness-chunk-11',
      'wpt-firefox-nightly-testharness-chunk-12',
      'wpt-firefox-nightly-testharness-chunk-13',
      'wpt-firefox-nightly-testharness-chunk-14',
      'wpt-firefox-nightly-testharness-chunk-15',
      'wpt-firefox-nightly-testharness-chunk-16',
      'wpt-chrome-dev-testharness-chunk-1',
      'wpt-chrome-dev-testharness-chunk-2',
      'wpt-chrome-dev-testharness-chunk-3',
      'wpt-chrome-dev-testharness-chunk-4',
      'wpt-chrome-dev-testharness-chunk-5',
      'wpt-chrome-dev-testharness-chunk-6',
      'wpt-chrome-dev-testharness-chunk-7',
      'wpt-chrome-dev-testharness-chunk-8',
      'wpt-chrome-dev-testharness-chunk-9',
      'wpt-chrome-dev-testharness-chunk-10',
      'wpt-chrome-dev-testharness-chunk-11',
      'wpt-chrome-dev-testharness-chunk-12',
      'wpt-chrome-dev-testharness-chunk-13',
      'wpt-chrome-dev-testharness-chunk-14',
      'wpt-chrome-dev-testharness-chunk-15',
      'wpt-chrome-dev-testharness-chunk-16',
      'wpt-firefox-nightly-reftest-chunk-1',
      'wpt-firefox-nightly-reftest-chunk-2',
      'wpt-firefox-nightly-reftest-chunk-3',
      'wpt-firefox-nightly-reftest-chunk-4',
      'wpt-firefox-nightly-reftest-chunk-5',
      'wpt-chrome-dev-reftest-chunk-1',
      'wpt-chrome-dev-reftest-chunk-2',
      'wpt-chrome-dev-reftest-chunk-3',
      'wpt-chrome-dev-reftest-chunk-4',
      'wpt-chrome-dev-reftest-chunk-5',
      'wpt-firefox-nightly-wdspec-chunk-1',
      'wpt-chrome-dev-wdspec-chunk-1',
      'lint'}),
    ("pr_event.json", True, {".taskcluster.yml",".travis.yml","tools/ci/start.sh"},
     {'lint',
      'tools/ unittests (Python 2)',
      'tools/ unittests (Python 3)',
      'tools/wpt/ tests',
      'resources/ tests',
      'infrastructure/ tests'}),
    # More tests are affected in the actual PR but it shouldn't affect the scheduled tasks
    ("pr_event_tests_affected.json", True, {"layout-instability/clip-negative-bottom-margin.html",
                                            "layout-instability/composited-element-movement.html"},
     {'download-firefox-nightly',
      'wpt-firefox-nightly-stability',
      'wpt-firefox-nightly-results',
      'wpt-firefox-nightly-results-without-changes',
      'wpt-chrome-dev-stability',
      'wpt-chrome-dev-results',
      'wpt-chrome-dev-results-without-changes',
      'lint'}),
])
def test_schedule_tasks(event_path, is_pr, files_changed, expected):
    with mock.patch("tools.ci.tc.decision.get_fetch_rev", return_value=(is_pr, None)):
        with mock.patch("tools.wpt.testfiles.repo_files_changed",
                        return_value=files_changed):
            with open(data_path(event_path)) as event_file:
                event = json.load(event_file)
                scheduled = decision.decide(event)
                assert set(scheduled.keys()) == expected
