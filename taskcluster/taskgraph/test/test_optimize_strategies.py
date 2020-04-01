# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import time

import pytest
from mozunit import main

from taskgraph.optimize.bugbug import BugBugPushSchedules, BugbugTimeoutException, platform
from taskgraph.task import Task


@pytest.fixture(scope='module')
def params():
    return {
        'head_repository': 'https://hg.mozilla.org/integration/autoland',
        'head_rev': 'abcdef',
        'branch': 'integration/autoland',
    }


@pytest.fixture(scope='module')
def generate_tasks():

    def inner(*tasks):
        for i, task in enumerate(tasks):
            task.setdefault('label', 'task-{}'.format(i))
            task.setdefault('kind', 'test')
            task.setdefault('attributes', {})
            task.setdefault('task', {})

            for attr in ('optimization', 'dependencies', 'soft_dependencies', 'release_artifacts'):
                task.setdefault(attr, None)

            task['task'].setdefault('label', task['label'])
            yield Task.from_json(task)

    return inner


@pytest.fixture(scope='module')
def tasks(generate_tasks):
    return list(generate_tasks(
        {'attributes': {'test_manifests': ['foo/test.ini', 'bar/test.ini']}},
        {'attributes': {'test_manifests': ['bar/test.ini'], 'build_type': 'debug'}},
        {'attributes': {'build_type': 'debug'}},
        {'attributes': {'test_manifests': []}},
    ))


def idfn(param):
    if isinstance(param, tuple):
        return param[0].__name__
    return None


@pytest.mark.parametrize("args,data,expected", [
    # empty
    pytest.param(
        (platform.all,),
        {},
        [],
    ),

    # only tasks without test manifests selected
    pytest.param(
        (platform.all,),
        {'tasks': {'task-1': 0.9, 'task-2': 0.1, 'task-3': 0.5}},
        ['task-2'],
    ),

    # tasks containing groups selected
    pytest.param(
        (platform.all,),
        {'groups': {'foo/test.ini': 0.4}},
        ['task-0'],
    ),

    # tasks matching "tasks" or "groups" selected
    pytest.param(
        (platform.all,),
        {'tasks': {'task-2': 0.2}, 'groups': {'foo/test.ini': 0.25, 'bar/test.ini': 0.75}},
        ['task-0', 'task-1', 'task-2'],
    ),

    # old format
    pytest.param(
        (platform.all,),
        {'tasks': ['task-2'], 'groups': ['foo/test.ini', 'bar/test.ini']},
        ['task-0', 'task-1', 'task-2'],
    ),

    # debug platform filter
    pytest.param(
        (platform.debug,),
        {'tasks': ['task-2', 'task-3'], 'groups': ['foo/test.ini', 'bar/test.ini']},
        ['task-1', 'task-2'],
    ),
], ids=idfn)
def test_bugbug_push_schedules(responses, params, tasks, args, data, expected):
    query = "/push/{branch}/{head_rev}/schedules".format(**params)
    url = BugBugPushSchedules.BUGBUG_BASE_URL + query

    responses.add(
        responses.GET,
        url,
        json=data,
        status=200,
    )

    opt = BugBugPushSchedules(*args)
    labels = [t.label for t in tasks if not opt.should_remove_task(t, params, None)]
    assert sorted(labels) == sorted(expected)


def test_bugbug_timeout(monkeypatch, responses, params, tasks):
    query = "/push/{branch}/{head_rev}/schedules".format(**params)
    url = BugBugPushSchedules.BUGBUG_BASE_URL + query
    responses.add(
        responses.GET,
        url,
        json={"ready": False},
        status=202,
    )

    # Make sure the test runs fast.
    monkeypatch.setattr(time, 'sleep', lambda i: None)

    opt = BugBugPushSchedules(platform.all)
    with pytest.raises(BugbugTimeoutException):
        opt.should_remove_task(tasks[0], params, None)


if __name__ == '__main__':
    main()
