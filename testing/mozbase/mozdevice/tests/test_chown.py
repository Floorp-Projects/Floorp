#!/usr/bin/env python

from __future__ import absolute_import

import logging

from mock import patch

import mozunit
import pytest


@pytest.mark.parametrize('boolean_value', [True, False])
def test_set_chown_r_attribute(mock_adb_object, redirect_stdout_and_assert, boolean_value):
    mock_adb_object._chown_R = boolean_value
    assert mock_adb_object._chown_R == boolean_value


def test_chown_path_internal(mock_adb_object, redirect_stdout_and_assert):
    """Tests whether attempt to chown internal path is ignored"""
    with patch.object(logging, 'getLogger') as mock_log:
        mock_adb_object._logger = mock_log

    testing_parameters = {
        "owner": "someuser",
        "path": "internal_storage",
    }
    expected = 'Ignoring attempt to chown external storage'
    mock_adb_object.chown(**testing_parameters)
    assert ''.join(mock_adb_object._logger.method_calls[0][1]) != ''
    assert ''.join(mock_adb_object._logger.method_calls[0][1]) == expected


def test_chown_one_path(mock_adb_object, redirect_stdout_and_assert):
    """Tests the path where only one path is provided."""
    # set up mock logging and self._chown_R attribute.
    with patch.object(logging, 'getLogger') as mock_log:
        mock_adb_object._logger = mock_log
    mock_adb_object._chown_R = True

    testing_parameters = {
        "owner": "someuser",
        "path": "/system",
    }
    command = 'chown {owner} {path}'.format(**testing_parameters)
    testing_parameters['text'] = command
    redirect_stdout_and_assert(mock_adb_object.chown, **testing_parameters)


def test_chown_one_path_with_group(mock_adb_object, redirect_stdout_and_assert):
    """Tests the path where group is provided."""
    # set up mock logging and self._chown_R attribute.
    with patch.object(logging, 'getLogger') as mock_log:
        mock_adb_object._logger = mock_log
    mock_adb_object._chown_R = True

    testing_parameters = {
        "owner": "someuser",
        "path": "/system",
        "group": "group_2",
    }
    command = 'chown {owner}.{group} {path}'.format(**testing_parameters)
    testing_parameters['text'] = command
    redirect_stdout_and_assert(mock_adb_object.chown, **testing_parameters)


if __name__ == '__main__':
    mozunit.main()
