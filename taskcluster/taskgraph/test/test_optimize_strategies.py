# Any copyright is dedicated to the public domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from __future__ import absolute_import

import time

import pytest
from datetime import datetime
from mozunit import main
from time import mktime

from taskgraph.optimize.backstop import Backstop
from taskgraph.optimize.bugbug import BugBugPushSchedules, BugbugTimeoutException, platform
from taskgraph.task import Task


@pytest.fixture(scope='module')
def params():
    return {
        'branch': 'integration/autoland',
        'head_repository': 'https://hg.mozilla.org/integration/autoland',
        'head_rev': 'abcdef',
        'project': 'autoland',
        'pushlog_id': 1,
        'pushdate': mktime(datetime.now().timetuple()),
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
        {'attributes': {'build_type': 'opt'}},
    ))


def idfn(param):
    if isinstance(param, tuple):
        return param[0].__name__
    return None


@pytest.mark.parametrize("args,data,expected", [
    # empty
    pytest.param(
        (platform.all, 0.1),
        {},
        [],
    ),

    # only tasks without test manifests selected
    pytest.param(
        (platform.all, 0.1),
        {'tasks': {'task-1': 0.9, 'task-2': 0.1, 'task-3': 0.5}},
        ['task-2'],
    ),

    # tasks containing groups selected
    pytest.param(
        (platform.all, 0.1),
        {'groups': {'foo/test.ini': 0.4}},
        ['task-0'],
    ),

    # tasks matching "tasks" or "groups" selected
    pytest.param(
        (platform.all, 0.1),
        {'tasks': {'task-2': 0.2}, 'groups': {'foo/test.ini': 0.25, 'bar/test.ini': 0.75}},
        ['task-0', 'task-1', 'task-2'],
    ),

    # tasks matching "tasks" or "groups" selected, when they exceed the confidence threshold
    pytest.param(
        (platform.all, 0.5),
        {
            'tasks': {'task-2': 0.2, 'task-4': 0.5},
            'groups': {'foo/test.ini': 0.65, 'bar/test.ini': 0.25}
        },
        ['task-0', 'task-4'],
    ),

    # tasks matching "reduced_tasks" are selected, when they exceed the confidence threshold
    pytest.param(
        (platform.all, 0.7, True),
        {
            'tasks': {'task-2': 0.7, 'task-4': 0.7},
            'reduced_tasks': {'task-4': 0.7},
            'groups': {'foo/test.ini': 0.75, 'bar/test.ini': 0.25}
        },
        ['task-4'],
    ),

    # debug platform filter
    pytest.param(
        (platform.debug, 0.5),
        {
            'tasks': {'task-2': 0.6, 'task-3': 0.6},
            'groups': {'foo/test.ini': 0.6, 'bar/test.ini': 0.6}
        },
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

    opt = BugBugPushSchedules(platform.all, 0.5)
    with pytest.raises(BugbugTimeoutException):
        opt.should_remove_task(tasks[0], params, None)


def test_backstop(params, tasks):
    all_labels = {t.label for t in tasks}
    opt = Backstop(10, 60)  # every 10th push or 1 hour

    # If there's no previous push date, run tasks
    params['pushlog_id'] = 8
    scheduled = {t.label for t in tasks if not opt.should_remove_task(t, params, None)}
    assert scheduled == all_labels

    # Only multiples of 10 schedule tasks. Pushdate from push 8 was cached.
    params['pushlog_id'] = 9
    params['pushdate'] += 3599
    scheduled = {t.label for t in tasks if not opt.should_remove_task(t, params, None)}
    assert scheduled == set()

    params['pushlog_id'] = 10
    params['pushdate'] += 1
    scheduled = {t.label for t in tasks if not opt.should_remove_task(t, params, None)}
    assert scheduled == all_labels

    # Tasks are also scheduled if an hour has passed.
    params['pushlog_id'] = 11
    params['pushdate'] += 3600
    scheduled = {t.label for t in tasks if not opt.should_remove_task(t, params, None)}
    assert scheduled == all_labels


if __name__ == '__main__':
    main()
