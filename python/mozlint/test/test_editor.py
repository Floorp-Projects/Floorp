# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import subprocess

import mozunit
import pytest

from mozlint import editor
from mozlint.result import ResultSummary, Issue

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def capture_commands(monkeypatch):

    def inner(commands):

        def fake_subprocess_call(*args, **kwargs):
            commands.append(args[0])

        monkeypatch.setattr(subprocess, 'call', fake_subprocess_call)

    return inner


@pytest.fixture
def result():
    result = ResultSummary()
    result.issues['foo.py'].extend([
        Issue(
            linter='no-foobar',
            path='foo.py',
            lineno=1,
            message="Oh no!",
        ),
        Issue(
            linter='no-foobar',
            path='foo.py',
            lineno=3,
            column=10,
            message="To Yuma!",
        ),
    ])
    return result


def test_no_editor(monkeypatch, capture_commands, result):
    commands = []
    capture_commands(commands)

    monkeypatch.delenv('EDITOR', raising=False)
    editor.edit_issues(result)
    assert commands == []


def test_no_issues(monkeypatch, capture_commands, result):
    commands = []
    capture_commands(commands)

    monkeypatch.setenv('EDITOR', 'generic')
    result.issues = {}
    editor.edit_issues(result)
    assert commands == []


def test_vim(monkeypatch, capture_commands, result):
    commands = []
    capture_commands(commands)

    monkeypatch.setenv('EDITOR', 'vim')
    editor.edit_issues(result)
    assert len(commands) == 1
    assert commands[0][0] == 'vim'


def test_generic(monkeypatch, capture_commands, result):
    commands = []
    capture_commands(commands)

    monkeypatch.setenv('EDITOR', 'generic')
    editor.edit_issues(result)
    assert len(commands) == len(result.issues)
    assert all(c[0] == 'generic' for c in commands)
    assert all('foo.py' in c for c in commands)


if __name__ == '__main__':
    mozunit.main()
