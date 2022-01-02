# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os

import mozunit
import pytest

from six.moves import reload_module as reload

from tryselect import push
from tryselect.selectors import again


@pytest.fixture(autouse=True)
def patch_history_path(tmpdir, monkeypatch):
    monkeypatch.setattr(push, "history_path", tmpdir.join("history.json").strpath)
    reload(again)


def test_try_again(monkeypatch):
    push.push_to_try(
        "fuzzy",
        "Fuzzy message",
        try_task_config=push.generate_try_task_config(
            "fuzzy",
            ["foo", "bar"],
            {"use-artifact-builds": True},
        ),
    )

    assert os.path.isfile(push.history_path)
    with open(push.history_path, "r") as fh:
        assert len(fh.readlines()) == 1

    def fake_push_to_try(*args, **kwargs):
        return args, kwargs

    monkeypatch.setattr(push, "push_to_try", fake_push_to_try)
    reload(again)

    args, kwargs = again.run()

    assert args[0] == "again"
    assert args[1] == "Fuzzy message"

    try_task_config = kwargs.pop("try_task_config")
    assert sorted(try_task_config.get("tasks")) == sorted(["foo", "bar"])
    assert try_task_config.get("env") == {"TRY_SELECTOR": "fuzzy"}
    assert try_task_config.get("use-artifact-builds")

    with open(push.history_path, "r") as fh:
        assert len(fh.readlines()) == 1


def test_no_push_does_not_generate_history(tmpdir):
    assert not os.path.isfile(push.history_path)

    push.push_to_try(
        "fuzzy",
        "Fuzzy",
        try_task_config=push.generate_try_task_config(
            "fuzzy",
            ["foo", "bar"],
            {"use-artifact-builds": True},
        ),
        push=False,
    )
    assert not os.path.isfile(push.history_path)
    assert again.run() == 1


if __name__ == "__main__":
    mozunit.main()
