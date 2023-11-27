# -*- coding: utf-8 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
from pathlib import Path

import pytest
from mozunit import main
from skipfails import Skipfails

DATA_PATH = Path(__file__).with_name("data")


class MockResult(object):
    def __init__(self, result):
        self.result = result

    @property
    def group(self):
        return self.result["group"]

    @property
    def ok(self):
        _ok = self.result["ok"]
        return _ok


class MockTask(object):
    def __init__(self, task):
        self.task = task
        if "results" in self.task:
            self.task["results"] = [
                MockResult(result) for result in self.task["results"]
            ]
        else:
            self.task["results"] = []

    @property
    def failure_types(self):
        if "failure_types" in self.task:
            return self.task["failure_types"]
        else:  # note no failure_types in Task object
            return {}

    @property
    def id(self):
        return self.task["id"]

    @property
    def label(self):
        return self.task["label"]

    @property
    def results(self):
        return self.task["results"]


def test_get_revision():
    """Test get_revision"""

    sf = Skipfails()
    with pytest.raises(ValueError) as e_info:
        sf.get_revision("")
    assert str(e_info.value) == "try_url scheme not https"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://foo.bar")
    assert str(e_info.value) == "try_url server not treeherder.mozilla.org"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://treeherder.mozilla.org")
    assert str(e_info.value) == "try_url query missing"

    with pytest.raises(ValueError) as e_info:
        sf.get_revision("https://treeherder.mozilla.org?a=1")
    assert str(e_info.value) == "try_url query missing revision"

    revision, repo = sf.get_revision(
        "https://treeherder.mozilla.org/jobs?repo=try&revision=5b1738d0af571777199ff3c694b1590ff574946b"
    )
    assert revision == "5b1738d0af571777199ff3c694b1590ff574946b"
    assert repo == "try"


def test_get_tasks():
    """Test get_tasks import of mozci"""

    from mozci.push import Push

    revision = "5b1738d0af571777199ff3c694b1590ff574946b"
    repo = "try"
    push = Push(revision, repo)
    assert push is not None


def test_get_failures_1():
    """Test get_failures 1"""

    tasks_name = "wayland-tasks-1.json"
    exp_f_name = "wayland-failures-1.json"
    sf = Skipfails()
    tasks_fp = DATA_PATH.joinpath(tasks_name).open("r", encoding="utf-8")
    tasks = json.load(tasks_fp)
    tasks = [MockTask(task) for task in tasks]
    exp_f_fp = DATA_PATH.joinpath(exp_f_name).open("r", encoding="utf-8")
    expected_failures = json.load(exp_f_fp)
    failures = sf.get_failures(tasks)
    assert len(failures) == len(expected_failures)
    for i in range(len(expected_failures)):
        assert failures[i]["manifest"] == expected_failures[i]["manifest"]
        assert failures[i]["path"] == expected_failures[i]["path"]
        assert failures[i]["classification"] == expected_failures[i]["classification"]


def test_get_failures_2():
    """Test get_failures 2"""

    tasks_name = "wayland-tasks-2.json"
    exp_f_name = "wayland-failures-2.json"
    sf = Skipfails()
    tasks_fp = DATA_PATH.joinpath(tasks_name).open("r", encoding="utf-8")
    tasks = json.load(tasks_fp)
    tasks = [MockTask(task) for task in tasks]
    exp_f_fp = DATA_PATH.joinpath(exp_f_name).open("r", encoding="utf-8")
    expected_failures = json.load(exp_f_fp)
    failures = sf.get_failures(tasks)
    assert len(failures) == len(expected_failures)
    for i in range(len(expected_failures)):
        assert failures[i]["manifest"] == expected_failures[i]["manifest"]
        assert failures[i]["path"] == expected_failures[i]["path"]
        assert failures[i]["classification"] == expected_failures[i]["classification"]


def test_get_failures_3():
    """Test get_failures 3"""

    tasks_name = "wayland-tasks-3.json"
    exp_f_name = "wayland-failures-3.json"
    sf = Skipfails()
    tasks_fp = DATA_PATH.joinpath(tasks_name).open("r", encoding="utf-8")
    tasks = json.load(tasks_fp)
    tasks = [MockTask(task) for task in tasks]
    exp_f_fp = DATA_PATH.joinpath(exp_f_name).open("r", encoding="utf-8")
    expected_failures = json.load(exp_f_fp)
    failures = sf.get_failures(tasks)
    assert len(failures) == len(expected_failures)
    for i in range(len(expected_failures)):
        assert failures[i]["manifest"] == expected_failures[i]["manifest"]
        assert failures[i]["path"] == expected_failures[i]["path"]
        assert failures[i]["classification"] == expected_failures[i]["classification"]


def test_get_bug():
    """Test get_bug"""

    sf = Skipfails()
    id = 1682371
    bug = sf.get_bug(id)
    assert bug.id == id
    assert bug.product == "Testing"
    assert bug.component == "General"
    assert (
        bug.summary
        == "create tool to quickly parse and identify all failures from a try push and ideally annotate manifests"
    )


if __name__ == "__main__":
    main()
