#!/usr/bin/env python

from __future__ import absolute_import

import pytest
import mozunit

from mozdevice import ADBError


def test_is_app_installed(mock_adb_object):
    """Tests that is_app_installed returns True if app is installed."""
    assert mock_adb_object.is_app_installed(
        'org.mozilla.geckoview_example')


def test_is_app_installed_not_installed(mock_adb_object):
    """Tests that is_app_installed returns False if provided app_name
    does not resolve."""
    assert not mock_adb_object.is_app_installed(
        'some_random_name')


def test_is_app_installed_partial_name(mock_adb_object):
    """Tests that is_app_installed returns False if provided app_name
    is only a partial match."""
    assert not mock_adb_object.is_app_installed(
        'fennec')


def test_is_app_installed_package_manager_error(mock_adb_object):
    """Tests that is_app_installed is able to raise an exception."""
    with pytest.raises(ADBError):
        mock_adb_object.is_app_installed('error')


def test_is_app_installed_no_installed_package_found(mock_adb_object):
    """Tests that is_app_installed is able to handle scenario
    where no installed packages are found."""
    assert not mock_adb_object.is_app_installed('none')


if __name__ == '__main__':
    mozunit.main()
