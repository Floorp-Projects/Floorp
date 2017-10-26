#!/usr/bin/env python

from __future__ import absolute_import, print_function

import os

import mozunit
import pytest
from mozdebug.mozdebug import (
    _DEBUGGER_PRIORITIES,
    DebuggerSearch,
    get_default_debugger_name,
)


here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def set_debuggers(monkeypatch):
    debugger_dir = os.path.join(here, 'fake_debuggers')

    def _set_debuggers(*debuggers):
        dirs = []
        for d in debuggers:
            if d.endswith('.exe'):
                d = d[:-len('.exe')]
            dirs.append(os.path.join(debugger_dir, d))
        monkeypatch.setenv('PATH', os.pathsep.join(dirs))

    return _set_debuggers


@pytest.mark.parametrize('os_name', ['android', 'linux', 'mac', 'win', 'unknown'])
def test_default_debugger_name(os_name, set_debuggers, monkeypatch):
    import mozinfo
    import sys

    def update_os_name(*args, **kwargs):
        mozinfo.info['os'] = os_name

    monkeypatch.setattr(mozinfo, 'find_and_update_from_json', update_os_name)

    if sys.platform == 'win32':
        # This is used so distutils.spawn.find_executable doesn't add '.exe'
        # suffixes to all our dummy binaries on Windows.
        monkeypatch.setattr(sys, 'platform', 'linux')

    debuggers = _DEBUGGER_PRIORITIES[os_name][:]
    debuggers.reverse()
    first = True
    while len(debuggers) > 0:
        set_debuggers(*debuggers)

        if first:
            assert get_default_debugger_name() == debuggers[-1]
            first = False
        else:
            assert get_default_debugger_name() is None
            assert get_default_debugger_name(DebuggerSearch.KeepLooking) == debuggers[-1]
        debuggers = debuggers[:-1]


if __name__ == '__main__':
    mozunit.main()
