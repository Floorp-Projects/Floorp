#!/usr/bin/env python

from __future__ import absolute_import

import mozunit


def test_escape_command_line(mock_adb_object, redirect_stdout_and_assert):
    """Test _escape_command_line."""
    cases = {
        # expected output : test input
        'adb shell ls -l': ['adb', 'shell', 'ls', '-l'],
        'adb shell "ls -l"': ['adb', 'shell', 'ls -l'],
        '-e "if (true)"': ['-e', 'if (true)'],
        '-e \'if (x === "hello")\'': ['-e', 'if (x === "hello")'],
        '-e "if (x === \'hello\')"': ['-e', "if (x === 'hello')"],
    }
    for expected, input in cases.items():
        assert mock_adb_object._escape_command_line(input) == expected


if __name__ == '__main__':
    mozunit.main()
