# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/


import time
from datetime import datetime
from time import mktime

import pytest
from mozunit import main
from taskgraph.task import Task

from gecko_taskgraph.optimize import project, registry
from gecko_taskgraph.optimize.strategies import IndexSearch, SkipUnlessSchedules
from gecko_taskgraph.optimize.backstop import SkipUnlessBackstop, SkipUnlessPushInterval
from gecko_taskgraph.optimize.bugbug import (
    BugBugPushSchedules,
    DisperseGroups,
    FALLBACK,
    SkipUnlessDebug,
)
from gecko_taskgraph.util.backstop import BACKSTOP_PUSH_INTERVAL
from gecko_taskgraph.util.bugbug import (
    BUGBUG_BASE_URL,
    BugbugTimeoutException,
    push_schedules,
)


@pytest.fixture(autouse=True)
def clear_push_schedules_memoize():
    push_schedules.clear()


@pytest.fixture
def params():
    return {
        "branch": "integration/autoland",
        "head_repository": "https://hg.mozilla.org/integration/autoland",
        "head_rev": "abcdef",
        "project": "autoland",
        "pushlog_id": 1,
        "pushdate": mktime(datetime.now().timetuple()),
    }


def generate_tasks(*tasks):
    for i, task in enumerate(tasks):
        task.setdefault("label", f"task-{i}-label")
        task.setdefault("kind", "test")
        task.setdefault("task", {})
        task.setdefault("attributes", {})
        task["attributes"].setdefault("e10s", True)

        for attr in (
            "optimization",
            "dependencies",
            "soft_dependencies",
        ):
            task.setdefault(attr, None)

        task["task"].setdefault("label", task["label"])
        yield Task.from_json(task)


# task sets

default_tasks = list(
    generate_tasks(
        {"attributes": {"test_manifests": ["foo/test.ini", "bar/test.ini"]}},
        {"attributes": {"test_manifests": ["bar/test.ini"], "build_type": "debug"}},
        {"attributes": {"build_type": "debug"}},
        {"attributes": {"test_manifests": [], "build_type": "opt"}},
        {"attributes": {"build_type": "opt"}},
    )
)


disperse_tasks = list(
    generate_tasks(
        {
            "attributes": {
                "test_manifests": ["foo/test.ini", "bar/test.ini"],
                "test_platform": "linux/opt",
            }
        },
        {
            "attributes": {
                "test_manifests": ["bar/test.ini"],
                "test_platform": "linux/opt",
            }
        },
        {
            "attributes": {
                "test_manifests": ["bar/test.ini"],
                "test_platform": "windows/debug",
            }
        },
        {
            "attributes": {
                "test_manifests": ["bar/test.ini"],
                "test_platform": "linux/opt",
                "unittest_variant": "no-fission",
            }
        },
        {
            "attributes": {
                "e10s": False,
                "test_manifests": ["bar/test.ini"],
                "test_platform": "linux/opt",
            }
        },
    )
)


def idfn(param):
    if isinstance(param, tuple):
        try:
            return param[0].__name__
        except AttributeError:
            return None
    return None


@pytest.mark.parametrize(
    "opt,tasks,arg,expected",
    [
        # debug
        pytest.param(
            SkipUnlessDebug(),
            default_tasks,
            None,
            ["task-0-label", "task-1-label", "task-2-label"],
        ),
        # disperse with no supplied importance
        pytest.param(
            DisperseGroups(),
            disperse_tasks,
            None,
            [t.label for t in disperse_tasks],
        ),
        # disperse with low importance
        pytest.param(
            DisperseGroups(),
            disperse_tasks,
            {"bar/test.ini": "low"},
            ["task-0-label", "task-2-label"],
        ),
        # disperse with medium importance
        pytest.param(
            DisperseGroups(),
            disperse_tasks,
            {"bar/test.ini": "medium"},
            ["task-0-label", "task-1-label", "task-2-label"],
        ),
        # disperse with high importance
        pytest.param(
            DisperseGroups(),
            disperse_tasks,
            {"bar/test.ini": "high"},
            ["task-0-label", "task-1-label", "task-2-label", "task-3-label"],
        ),
    ],
    ids=idfn,
)
def test_optimization_strategy_remove(params, opt, tasks, arg, expected):
    labels = [t.label for t in tasks if not opt.should_remove_task(t, params, arg)]
    assert sorted(labels) == sorted(expected)


@pytest.mark.parametrize(
    "state,expires,expected",
    (
        ("completed", "2021-06-06T14:53:16.937Z", False),
        ("completed", "2021-06-08T14:53:16.937Z", "abc"),
        ("exception", "2021-06-08T14:53:16.937Z", False),
        ("failed", "2021-06-08T14:53:16.937Z", False),
    ),
)
def test_index_search(responses, params, state, expires, expected):
    taskid = "abc"
    index_path = "foo.bar.latest"
    responses.add(
        responses.GET,
        f"https://firefox-ci-tc.services.mozilla.com/api/index/v1/task/{index_path}",
        json={"taskId": taskid},
        status=200,
    )

    responses.add(
        responses.GET,
        f"https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/{taskid}/status",
        json={
            "status": {
                "state": state,
                "expires": expires,
            }
        },
        status=200,
    )

    opt = IndexSearch()
    deadline = "2021-06-07T19:03:20.482Z"
    assert opt.should_replace_task({}, params, deadline, (index_path,)) == expected


@pytest.mark.parametrize(
    "args,data,expected",
    [
        # empty
        pytest.param(
            (0.1,),
            {},
            [],
        ),
        # only tasks without test manifests selected
        pytest.param(
            (0.1,),
            {"tasks": {"task-1-label": 0.9, "task-2-label": 0.1, "task-3-label": 0.5}},
            ["task-2-label"],
        ),
        # tasks which are unknown to bugbug are selected
        pytest.param(
            (0.1,),
            {
                "tasks": {"task-1-label": 0.9, "task-3-label": 0.5},
                "known_tasks": ["task-1-label", "task-3-label", "task-4-label"],
            },
            ["task-2-label"],
        ),
        # tasks containing groups selected
        pytest.param(
            (0.1,),
            {"groups": {"foo/test.ini": 0.4}},
            ["task-0-label"],
        ),
        # tasks matching "tasks" or "groups" selected
        pytest.param(
            (0.1,),
            {
                "tasks": {"task-2-label": 0.2},
                "groups": {"foo/test.ini": 0.25, "bar/test.ini": 0.75},
            },
            ["task-0-label", "task-1-label", "task-2-label"],
        ),
        # tasks matching "tasks" or "groups" selected, when they exceed the confidence threshold
        pytest.param(
            (0.5,),
            {
                "tasks": {"task-2-label": 0.2, "task-4-label": 0.5},
                "groups": {"foo/test.ini": 0.65, "bar/test.ini": 0.25},
            },
            ["task-0-label", "task-4-label"],
        ),
        # tasks matching "reduced_tasks" are selected, when they exceed the confidence threshold
        pytest.param(
            (0.7, True, True),
            {
                "tasks": {"task-2-label": 0.7, "task-4-label": 0.7},
                "reduced_tasks": {"task-4-label": 0.7},
                "groups": {"foo/test.ini": 0.75, "bar/test.ini": 0.25},
            },
            ["task-4-label"],
        ),
        # tasks matching "groups" selected, only on specific platforms.
        pytest.param(
            (0.1, False, False, None, 1, True),
            {
                "tasks": {"task-2-label": 0.2},
                "groups": {"foo/test.ini": 0.25, "bar/test.ini": 0.75},
                "config_groups": {
                    "foo/test.ini": ["task-1-label", "task-0-label"],
                    "bar/test.ini": ["task-0-label"],
                },
            },
            ["task-0-label", "task-2-label"],
        ),
        pytest.param(
            (0.1, False, False, None, 1, True),
            {
                "tasks": {"task-2-label": 0.2},
                "groups": {"foo/test.ini": 0.25, "bar/test.ini": 0.75},
                "config_groups": {
                    "foo/test.ini": ["task-1-label", "task-0-label"],
                    "bar/test.ini": ["task-1-label"],
                },
            },
            ["task-0-label", "task-1-label", "task-2-label"],
        ),
        pytest.param(
            (0.1, False, False, None, 1, True),
            {
                "tasks": {"task-2-label": 0.2},
                "groups": {"foo/test.ini": 0.25, "bar/test.ini": 0.75},
                "config_groups": {
                    "foo/test.ini": ["task-1-label"],
                    "bar/test.ini": ["task-0-label"],
                },
            },
            ["task-0-label", "task-2-label"],
        ),
        pytest.param(
            (0.1, False, False, None, 1, True),
            {
                "tasks": {"task-2-label": 0.2},
                "groups": {"foo/test.ini": 0.25, "bar/test.ini": 0.75},
                "config_groups": {
                    "foo/test.ini": ["task-1-label"],
                    "bar/test.ini": ["task-3-label"],
                },
            },
            ["task-2-label"],
        ),
    ],
    ids=idfn,
)
def test_bugbug_push_schedules(responses, params, args, data, expected):
    query = "/push/{branch}/{head_rev}/schedules".format(**params)
    url = BUGBUG_BASE_URL + query

    responses.add(
        responses.GET,
        url,
        json=data,
        status=200,
    )

    opt = BugBugPushSchedules(*args)
    labels = [
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    ]
    assert sorted(labels) == sorted(expected)


def test_bugbug_multiple_pushes(responses, params):
    pushes = {str(pid): {"changesets": [f"c{pid}"]} for pid in range(8, 10)}

    responses.add(
        responses.GET,
        "https://hg.mozilla.org/integration/autoland/json-pushes/?version=2&startID=8&endID=9",
        json={"pushes": pushes},
        status=200,
    )

    responses.add(
        responses.GET,
        BUGBUG_BASE_URL + "/push/{}/c9/schedules".format(params["branch"]),
        json={
            "tasks": {"task-2-label": 0.2, "task-4-label": 0.5},
            "groups": {"foo/test.ini": 0.2, "bar/test.ini": 0.25},
            "config_groups": {"foo/test.ini": ["linux-*"], "bar/test.ini": ["task-*"]},
            "known_tasks": ["task-4-label"],
        },
        status=200,
    )

    # Tasks with a lower confidence don't override task with a higher one.
    # Tasks with a higher confidence override tasks with a lower one.
    # Known tasks are merged.
    responses.add(
        responses.GET,
        BUGBUG_BASE_URL + "/push/{branch}/{head_rev}/schedules".format(**params),
        json={
            "tasks": {"task-2-label": 0.2, "task-4-label": 0.2},
            "groups": {"foo/test.ini": 0.65, "bar/test.ini": 0.25},
            "config_groups": {
                "foo/test.ini": ["task-*"],
                "bar/test.ini": ["windows-*"],
            },
            "known_tasks": ["task-1-label", "task-3-label"],
        },
        status=200,
    )

    params["pushlog_id"] = 10

    opt = BugBugPushSchedules(0.3, False, False, False, 2)
    labels = [
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    ]
    assert sorted(labels) == sorted(["task-0-label", "task-2-label", "task-4-label"])

    opt = BugBugPushSchedules(0.3, False, False, False, 2, True)
    labels = [
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    ]
    assert sorted(labels) == sorted(["task-0-label", "task-2-label", "task-4-label"])

    opt = BugBugPushSchedules(0.2, False, False, False, 2, True)
    labels = [
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    ]
    assert sorted(labels) == sorted(
        ["task-0-label", "task-1-label", "task-2-label", "task-4-label"]
    )


def test_bugbug_timeout(monkeypatch, responses, params):
    query = "/push/{branch}/{head_rev}/schedules".format(**params)
    url = BUGBUG_BASE_URL + query
    responses.add(
        responses.GET,
        url,
        json={"ready": False},
        status=202,
    )

    # Make sure the test runs fast.
    monkeypatch.setattr(time, "sleep", lambda i: None)

    opt = BugBugPushSchedules(0.5)
    with pytest.raises(BugbugTimeoutException):
        opt.should_remove_task(default_tasks[0], params, None)


def test_bugbug_fallback(monkeypatch, responses, params):
    query = "/push/{branch}/{head_rev}/schedules".format(**params)
    url = BUGBUG_BASE_URL + query
    responses.add(
        responses.GET,
        url,
        json={"ready": False},
        status=202,
    )

    opt = BugBugPushSchedules(0.5, fallback=FALLBACK)

    # Make sure the test runs fast.
    monkeypatch.setattr(time, "sleep", lambda i: None)

    def fake_should_remove_task(task, params, _):
        return task.label == default_tasks[0].label

    monkeypatch.setattr(
        registry[FALLBACK], "should_remove_task", fake_should_remove_task
    )

    assert opt.should_remove_task(default_tasks[0], params, None)

    # Make sure we don't hit bugbug more than once.
    responses.reset()

    assert not opt.should_remove_task(default_tasks[1], params, None)


def test_backstop(params):
    all_labels = {t.label for t in default_tasks}
    opt = SkipUnlessBackstop()

    params["backstop"] = False
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == set()

    params["backstop"] = True
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == all_labels


def test_push_interval(params):
    all_labels = {t.label for t in default_tasks}
    opt = SkipUnlessPushInterval(10)  # every 10th push

    # Only multiples of 10 schedule tasks.
    params["pushlog_id"] = 9
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == set()

    params["pushlog_id"] = 10
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == all_labels


def test_expanded(params):
    all_labels = {t.label for t in default_tasks}
    opt = registry["skip-unless-expanded"]

    params["backstop"] = False
    params["pushlog_id"] = BACKSTOP_PUSH_INTERVAL / 2
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == all_labels

    params["pushlog_id"] += 1
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == set()

    params["backstop"] = True
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, None)
    }
    assert scheduled == all_labels


def test_project_autoland_test(monkeypatch, responses, params):
    """Tests the behaviour of the `project.autoland["test"]` strategy on
    various types of pushes.
    """
    # This is meant to test the composition of substrategies, and not the
    # actual optimization implementations. So mock them out for simplicity.
    monkeypatch.setattr(SkipUnlessSchedules, "should_remove_task", lambda *args: False)
    monkeypatch.setattr(DisperseGroups, "should_remove_task", lambda *args: False)

    def fake_bugbug_should_remove_task(self, task, params, importance):
        if self.num_pushes > 1:
            return task.label == "task-4-label"
        return task.label in ("task-2-label", "task-3-label", "task-4-label")

    monkeypatch.setattr(
        BugBugPushSchedules, "should_remove_task", fake_bugbug_should_remove_task
    )

    opt = project.autoland["test"]

    # On backstop pushes, nothing gets optimized.
    params["backstop"] = True
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    }
    assert scheduled == {t.label for t in default_tasks}

    # On expanded pushes, some things are optimized.
    params["backstop"] = False
    params["pushlog_id"] = 10
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    }
    assert scheduled == {"task-0-label", "task-1-label", "task-2-label", "task-3-label"}

    # On regular pushes, more things are optimized.
    params["pushlog_id"] = 11
    scheduled = {
        t.label for t in default_tasks if not opt.should_remove_task(t, params, {})
    }
    assert scheduled == {"task-0-label", "task-1-label"}


if __name__ == "__main__":
    main()
