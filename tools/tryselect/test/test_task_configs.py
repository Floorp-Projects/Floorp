# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import inspect
from argparse import ArgumentParser
from textwrap import dedent

import mozunit
import pytest
from tryselect.task_config import Pernosco, all_task_configs

TC_URL = "https://firefox-ci-tc.services.mozilla.com"
TH_URL = "https://treeherder.mozilla.org"

# task configs have a list of tests of the form (input, expected)
TASK_CONFIG_TESTS = {
    "artifact": [
        (["--no-artifact"], None),
        (
            ["--artifact"],
            {"try_task_config": {"use-artifact-builds": True, "disable-pgo": True}},
        ),
    ],
    "chemspill-prio": [
        ([], None),
        (["--chemspill-prio"], {"try_task_config": {"chemspill-prio": True}}),
    ],
    "env": [
        ([], None),
        (
            ["--env", "foo=bar", "--env", "num=10"],
            {"try_task_config": {"env": {"foo": "bar", "num": "10"}}},
        ),
    ],
    "path": [
        ([], None),
        (
            ["dom/indexedDB"],
            {
                "try_task_config": {
                    "env": {"MOZHARNESS_TEST_PATHS": '{"xpcshell": ["dom/indexedDB"]}'}
                }
            },
        ),
        (
            ["dom/indexedDB", "testing"],
            {
                "try_task_config": {
                    "env": {
                        "MOZHARNESS_TEST_PATHS": '{"xpcshell": ["dom/indexedDB", "testing"]}'
                    }
                }
            },
        ),
        (["invalid/path"], SystemExit),
    ],
    "pernosco": [
        ([], None),
    ],
    "rebuild": [
        ([], None),
        (["--rebuild", "10"], {"try_task_config": {"rebuild": 10}}),
        (["--rebuild", "1"], SystemExit),
        (["--rebuild", "21"], SystemExit),
    ],
    "worker-overrides": [
        ([], None),
        (
            ["--worker-override", "alias=worker/pool"],
            {"try_task_config": {"worker-overrides": {"alias": "worker/pool"}}},
        ),
        (
            [
                "--worker-override",
                "alias=worker/pool",
                "--worker-override",
                "alias=other/pool",
            ],
            SystemExit,
        ),
        (
            ["--worker-suffix", "b-linux=-dev"],
            {
                "try_task_config": {
                    "worker-overrides": {"b-linux": "gecko-1/b-linux-dev"}
                }
            },
        ),
        (
            [
                "--worker-override",
                "b-linux=worker/pool" "--worker-suffix",
                "b-linux=-dev",
            ],
            SystemExit,
        ),
    ],
    "new-test-config": [
        ([], None),
        (["--new-test-config"], {"try_task_config": {"new-test-config": True}}),
    ],
}


@pytest.fixture
def config_patch_resolver(patch_resolver):
    def inner(paths):
        patch_resolver(
            [], [{"flavor": "xpcshell", "srcdir_relpath": path} for path in paths]
        )

    return inner


def test_task_configs(config_patch_resolver, task_config, args, expected):
    parser = ArgumentParser()

    cfg = all_task_configs[task_config]()
    cfg.add_arguments(parser)

    if inspect.isclass(expected) and issubclass(expected, BaseException):
        with pytest.raises(expected):
            args = parser.parse_args(args)
            if task_config == "path":
                config_patch_resolver(**vars(args))

            cfg.get_parameters(**vars(args))
    else:
        args = parser.parse_args(args)
        if task_config == "path":
            config_patch_resolver(**vars(args))

        params = cfg.get_parameters(**vars(args))
        assert params == expected


@pytest.fixture
def patch_ssh_user(mocker):
    def inner(user):
        mock_stdout = mocker.Mock()
        mock_stdout.stdout = dedent(
            f"""
            key1 foo
            user {user}
            key2 bar
            """
        )
        return mocker.patch(
            "tryselect.util.ssh.subprocess.run", return_value=mock_stdout
        )

    return inner


def test_pernosco(patch_ssh_user):
    patch_ssh_user("user@mozilla.com")
    parser = ArgumentParser()

    cfg = Pernosco()
    cfg.add_arguments(parser)
    args = parser.parse_args(["--pernosco"])
    params = cfg.get_parameters(**vars(args))
    assert params == {"try_task_config": {"env": {"PERNOSCO": "1"}, "pernosco": True}}


def test_exisiting_tasks(responses, patch_ssh_user):
    parser = ArgumentParser()
    cfg = all_task_configs["existing-tasks"]()
    cfg.add_arguments(parser)

    user = "user@example.com"
    rev = "a" * 40
    task_id = "abc"
    label_to_taskid = {"task-foo": "123", "task-bar": "456"}

    args = ["--use-existing-tasks"]
    args = parser.parse_args(args)

    responses.add(
        responses.GET,
        f"{TH_URL}/api/project/try/push/?count=1&author={user}",
        json={"meta": {"count": 1}, "results": [{"revision": rev}]},
    )

    responses.add(
        responses.GET,
        f"{TC_URL}/api/index/v1/task/gecko.v2.try.revision.{rev}.taskgraph.decision",
        json={"taskId": task_id},
    )

    responses.add(
        responses.GET,
        f"{TC_URL}/api/queue/v1/task/{task_id}/artifacts/public/label-to-taskid.json",
        json=label_to_taskid,
    )

    m = patch_ssh_user(user)
    params = cfg.get_parameters(**vars(args))
    assert params == {"existing_tasks": label_to_taskid}

    m.assert_called_once_with(
        ["ssh", "-G", "hg.mozilla.org"], text=True, check=True, capture_output=True
    )


def test_exisiting_tasks_task_id(responses):
    parser = ArgumentParser()
    cfg = all_task_configs["existing-tasks"]()
    cfg.add_arguments(parser)

    task_id = "abc"
    label_to_taskid = {"task-foo": "123", "task-bar": "456"}

    args = ["--use-existing-tasks", f"task-id={task_id}"]
    args = parser.parse_args(args)

    responses.add(
        responses.GET,
        f"{TC_URL}/api/queue/v1/task/{task_id}/artifacts/public/label-to-taskid.json",
        json=label_to_taskid,
    )

    params = cfg.get_parameters(**vars(args))
    assert params == {"existing_tasks": label_to_taskid}


def test_exisiting_tasks_rev(responses):
    parser = ArgumentParser()
    cfg = all_task_configs["existing-tasks"]()
    cfg.add_arguments(parser)

    rev = "aaaaaa"
    task_id = "abc"
    label_to_taskid = {"task-foo": "123", "task-bar": "456"}

    args = ["--use-existing-tasks", f"rev={rev}"]
    args = parser.parse_args(args)

    responses.add(
        responses.GET,
        f"{TC_URL}/api/index/v1/task/gecko.v2.try.revision.{rev}.taskgraph.decision",
        json={"taskId": task_id},
    )

    responses.add(
        responses.GET,
        f"{TC_URL}/api/queue/v1/task/{task_id}/artifacts/public/label-to-taskid.json",
        json=label_to_taskid,
    )

    params = cfg.get_parameters(**vars(args))
    assert params == {"existing_tasks": label_to_taskid}


if __name__ == "__main__":
    mozunit.main()
