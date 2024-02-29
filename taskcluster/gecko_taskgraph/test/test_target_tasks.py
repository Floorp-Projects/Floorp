# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


import contextlib
import re
import unittest

import pytest
from mozunit import main
from taskgraph.graph import Graph
from taskgraph.task import Task
from taskgraph.taskgraph import TaskGraph

from gecko_taskgraph import target_tasks, try_option_syntax


class FakeTryOptionSyntax:
    def __init__(self, message, task_graph, graph_config):
        self.trigger_tests = 0
        self.talos_trigger_tests = 0
        self.raptor_trigger_tests = 0
        self.notifications = None
        self.env = []
        self.profile = False
        self.tag = None
        self.no_retry = False

    def task_matches(self, task):
        return "at-at" in task.attributes


class TestTargetTasks(unittest.TestCase):
    def default_matches_project(self, run_on_projects, project):
        return self.default_matches(
            attributes={
                "run_on_projects": run_on_projects,
            },
            parameters={
                "project": project,
                "hg_branch": "default",
            },
        )

    def default_matches_hg_branch(self, run_on_hg_branches, hg_branch):
        attributes = {"run_on_projects": ["all"]}
        if run_on_hg_branches is not None:
            attributes["run_on_hg_branches"] = run_on_hg_branches

        return self.default_matches(
            attributes=attributes,
            parameters={
                "project": "mozilla-central",
                "hg_branch": hg_branch,
            },
        )

    def default_matches(self, attributes, parameters):
        method = target_tasks.get_method("default")
        graph = TaskGraph(
            tasks={
                "a": Task(kind="build", label="a", attributes=attributes, task={}),
            },
            graph=Graph(nodes={"a"}, edges=set()),
        )
        return "a" in method(graph, parameters, {})

    def test_default_all(self):
        """run_on_projects=[all] includes release, integration, and other projects"""
        self.assertTrue(self.default_matches_project(["all"], "mozilla-central"))
        self.assertTrue(self.default_matches_project(["all"], "baobab"))

    def test_default_integration(self):
        """run_on_projects=[integration] includes integration projects"""
        self.assertFalse(
            self.default_matches_project(["integration"], "mozilla-central")
        )
        self.assertFalse(self.default_matches_project(["integration"], "baobab"))

    def test_default_release(self):
        """run_on_projects=[release] includes release projects"""
        self.assertTrue(self.default_matches_project(["release"], "mozilla-central"))
        self.assertFalse(self.default_matches_project(["release"], "baobab"))

    def test_default_nothing(self):
        """run_on_projects=[] includes nothing"""
        self.assertFalse(self.default_matches_project([], "mozilla-central"))
        self.assertFalse(self.default_matches_project([], "baobab"))

    def test_default_hg_branch(self):
        self.assertTrue(self.default_matches_hg_branch(None, "default"))
        self.assertTrue(self.default_matches_hg_branch(None, "GECKOVIEW_62_RELBRANCH"))

        self.assertFalse(self.default_matches_hg_branch([], "default"))
        self.assertFalse(self.default_matches_hg_branch([], "GECKOVIEW_62_RELBRANCH"))

        self.assertTrue(self.default_matches_hg_branch(["all"], "default"))
        self.assertTrue(
            self.default_matches_hg_branch(["all"], "GECKOVIEW_62_RELBRANCH")
        )

        self.assertTrue(self.default_matches_hg_branch(["default"], "default"))
        self.assertTrue(self.default_matches_hg_branch([r"default"], "default"))
        self.assertFalse(
            self.default_matches_hg_branch([r"default"], "GECKOVIEW_62_RELBRANCH")
        )

        self.assertTrue(
            self.default_matches_hg_branch(
                ["GECKOVIEW_62_RELBRANCH"], "GECKOVIEW_62_RELBRANCH"
            )
        )
        self.assertTrue(
            self.default_matches_hg_branch(
                [r"GECKOVIEW_\d+_RELBRANCH"], "GECKOVIEW_62_RELBRANCH"
            )
        )
        self.assertTrue(
            self.default_matches_hg_branch(
                [r"GECKOVIEW_\d+_RELBRANCH"], "GECKOVIEW_62_RELBRANCH"
            )
        )
        self.assertFalse(
            self.default_matches_hg_branch([r"GECKOVIEW_\d+_RELBRANCH"], "default")
        )

    def make_task_graph(self):
        tasks = {
            "a": Task(kind=None, label="a", attributes={}, task={}),
            "b": Task(kind=None, label="b", attributes={"at-at": "yep"}, task={}),
            "c": Task(
                kind=None, label="c", attributes={"run_on_projects": ["try"]}, task={}
            ),
            "ddd-1": Task(kind="test", label="ddd-1", attributes={}, task={}),
            "ddd-2": Task(kind="test", label="ddd-2", attributes={}, task={}),
            "ddd-1-cf": Task(kind="test", label="ddd-1-cf", attributes={}, task={}),
            "ddd-2-cf": Task(kind="test", label="ddd-2-cf", attributes={}, task={}),
            "ddd-var-1": Task(kind="test", label="ddd-var-1", attributes={}, task={}),
            "ddd-var-2": Task(kind="test", label="ddd-var-2", attributes={}, task={}),
        }
        graph = Graph(
            nodes=set(
                [
                    "a",
                    "b",
                    "c",
                    "ddd-1",
                    "ddd-2",
                    "ddd-1-cf",
                    "ddd-2-cf",
                    "ddd-var-1",
                    "ddd-var-2",
                ]
            ),
            edges=set(),
        )
        return TaskGraph(tasks, graph)

    @contextlib.contextmanager
    def fake_TryOptionSyntax(self):
        orig_TryOptionSyntax = try_option_syntax.TryOptionSyntax
        try:
            try_option_syntax.TryOptionSyntax = FakeTryOptionSyntax
            yield
        finally:
            try_option_syntax.TryOptionSyntax = orig_TryOptionSyntax

    def test_empty_try(self):
        "try_mode = None runs nothing"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        params = {
            "try_mode": None,
            "project": "try",
            "message": "",
        }
        # only runs the task with run_on_projects: try
        self.assertEqual(method(tg, params, {}), [])

    def test_try_option_syntax(self):
        "try_mode = try_option_syntax uses TryOptionSyntax"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        with self.fake_TryOptionSyntax():
            params = {
                "try_mode": "try_option_syntax",
                "message": "try: -p all",
            }
            self.assertEqual(method(tg, params, {}), ["b"])

    def test_try_task_config(self):
        "try_mode = try_task_config uses the try config"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        params = {
            "try_mode": "try_task_config",
            "try_task_config": {"tasks": ["a"]},
        }
        self.assertEqual(method(tg, params, {}), ["a"])

    def test_try_task_config_regex(self):
        "try_mode = try_task_config uses the try config with regex instead of chunk numbers"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        params = {
            "try_mode": "try_task_config",
            "try_task_config": {"new-test-config": True, "tasks": ["ddd-*"]},
            "project": "try",
        }
        self.assertEqual(sorted(method(tg, params, {})), ["ddd-1", "ddd-2"])

    def test_try_task_config_regex_with_paths(self):
        "try_mode = try_task_config uses the try config with regex instead of chunk numbers"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        params = {
            "try_mode": "try_task_config",
            "try_task_config": {
                "new-test-config": True,
                "tasks": ["ddd-*"],
                "env": {"MOZHARNESS_TEST_PATHS": "foo/bar"},
            },
            "project": "try",
        }
        self.assertEqual(sorted(method(tg, params, {})), ["ddd-1"])

    def test_try_task_config_absolute(self):
        "try_mode = try_task_config uses the try config with full task labels"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        params = {
            "try_mode": "try_task_config",
            "try_task_config": {
                "new-test-config": True,
                "tasks": ["ddd-var-2", "ddd-1"],
            },
            "project": "try",
        }
        self.assertEqual(sorted(method(tg, params, {})), ["ddd-1", "ddd-var-2"])

    def test_try_task_config_regex_var(self):
        "try_mode = try_task_config uses the try config with regex instead of chunk numbers and a test variant"
        tg = self.make_task_graph()
        method = target_tasks.get_method("try_tasks")
        params = {
            "try_mode": "try_task_config",
            "try_task_config": {"new-test-config": True, "tasks": ["ddd-var-*"]},
            "project": "try",
        }
        self.assertEqual(sorted(method(tg, params, {})), ["ddd-var-1", "ddd-var-2"])


# tests for specific filters


@pytest.mark.parametrize(
    "name,params,expected",
    (
        pytest.param(
            "filter_tests_without_manifests",
            {
                "task": Task(kind="test", label="a", attributes={}, task={}),
                "parameters": None,
            },
            True,
            id="filter_tests_without_manifests_not_in_attributes",
        ),
        pytest.param(
            "filter_tests_without_manifests",
            {
                "task": Task(
                    kind="test",
                    label="a",
                    attributes={"test_manifests": ["foo"]},
                    task={},
                ),
                "parameters": None,
            },
            True,
            id="filter_tests_without_manifests_has_test_manifests",
        ),
        pytest.param(
            "filter_tests_without_manifests",
            {
                "task": Task(
                    kind="build",
                    label="a",
                    attributes={"test_manifests": None},
                    task={},
                ),
                "parameters": None,
            },
            True,
            id="filter_tests_without_manifests_not_a_test",
        ),
        pytest.param(
            "filter_tests_without_manifests",
            {
                "task": Task(
                    kind="test", label="a", attributes={"test_manifests": None}, task={}
                ),
                "parameters": None,
            },
            False,
            id="filter_tests_without_manifests_has_no_test_manifests",
        ),
        pytest.param(
            "filter_by_regex",
            {
                "task_label": "build-linux64-debug",
                "regexes": [re.compile("build")],
                "mode": "include",
            },
            True,
            id="filter_regex_simple_include",
        ),
        pytest.param(
            "filter_by_regex",
            {
                "task_label": "build-linux64-debug",
                "regexes": [re.compile("linux(.+)debug")],
                "mode": "include",
            },
            True,
            id="filter_regex_re_include",
        ),
        pytest.param(
            "filter_by_regex",
            {
                "task_label": "build-linux64-debug",
                "regexes": [re.compile("nothing"), re.compile("linux(.+)debug")],
                "mode": "include",
            },
            True,
            id="filter_regex_re_include_multiple",
        ),
        pytest.param(
            "filter_by_regex",
            {
                "task_label": "build-linux64-debug",
                "regexes": [re.compile("build")],
                "mode": "exclude",
            },
            False,
            id="filter_regex_simple_exclude",
        ),
        pytest.param(
            "filter_by_regex",
            {
                "task_label": "build-linux64-debug",
                "regexes": [re.compile("linux(.+)debug")],
                "mode": "exclude",
            },
            False,
            id="filter_regex_re_exclude",
        ),
        pytest.param(
            "filter_by_regex",
            {
                "task_label": "build-linux64-debug",
                "regexes": [re.compile("linux(.+)debug"), re.compile("nothing")],
                "mode": "exclude",
            },
            False,
            id="filter_regex_re_exclude_multiple",
        ),
        pytest.param(
            "filter_unsupported_artifact_builds",
            {
                "task": Task(
                    kind="test",
                    label="a",
                    attributes={"supports-artifact-builds": False},
                    task={},
                ),
                "parameters": {
                    "try_task_config": {
                        "use-artifact-builds": False,
                    },
                },
            },
            True,
            id="filter_unsupported_artifact_builds_no_artifact_builds",
        ),
        pytest.param(
            "filter_unsupported_artifact_builds",
            {
                "task": Task(
                    kind="test",
                    label="a",
                    attributes={"supports-artifact-builds": False},
                    task={},
                ),
                "parameters": {
                    "try_task_config": {
                        "use-artifact-builds": True,
                    },
                },
            },
            False,
            id="filter_unsupported_artifact_builds_removed",
        ),
        pytest.param(
            "filter_unsupported_artifact_builds",
            {
                "task": Task(
                    kind="test",
                    label="a",
                    attributes={"supports-artifact-builds": True},
                    task={},
                ),
                "parameters": {
                    "try_task_config": {
                        "use-artifact-builds": True,
                    },
                },
            },
            True,
            id="filter_unsupported_artifact_builds_not_removed",
        ),
        pytest.param(
            "filter_unsupported_artifact_builds",
            {
                "task": Task(kind="test", label="a", attributes={}, task={}),
                "parameters": {
                    "try_task_config": {
                        "use-artifact-builds": True,
                    },
                },
            },
            True,
            id="filter_unsupported_artifact_builds_not_removed",
        ),
    ),
)
def test_filters(name, params, expected):
    func = getattr(target_tasks, name)
    assert func(**params) is expected


if __name__ == "__main__":
    main()
