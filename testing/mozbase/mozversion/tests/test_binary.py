#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import os
import sys
import shutil

import mozunit
import pytest
from moztest.selftest.fixtures import binary as real_binary  # noqa: F401

from mozversion import errors, get_version


"""test getting application version information from a binary path"""


@pytest.fixture(name='binary')
def fixure_binary(tmpdir):
    binary = tmpdir.join('binary')
    binary.write('foobar')
    return str(binary)


@pytest.fixture(name='application_ini')
def fixture_application_ini(tmpdir):
    ini = tmpdir.join('application.ini')
    ini.write("""[App]
ID = AppID
Name = AppName
CodeName = AppCodeName
Version = AppVersion
BuildID = AppBuildID
SourceRepository = AppSourceRepo
SourceStamp = AppSourceStamp
Vendor = AppVendor""")
    return str(ini)


@pytest.fixture(name='platform_ini')
def fixture_platform_ini(tmpdir):
    ini = tmpdir.join('platform.ini')
    ini.write("""[Build]
BuildID = PlatformBuildID
Milestone = PlatformMilestone
SourceStamp = PlatformSourceStamp
SourceRepository = PlatformSourceRepo""")
    return str(ini)


def test_real_binary(real_binary):  # noqa: F811
    if not real_binary:
        pytest.skip('No binary found')
    v = get_version(real_binary)
    assert isinstance(v, dict)


def test_binary(binary, application_ini, platform_ini):
    _check_version(get_version(binary))


@pytest.mark.skipif(
    not hasattr(os, 'symlink'),
    reason='os.symlink not supported on this platform')
def test_symlinked_binary(binary, application_ini, platform_ini, tmpdir):
    # create a symlink of the binary in another directory and check
    # version against this symlink
    symlink = str(tmpdir.join('symlink'))
    os.symlink(binary, symlink)
    _check_version(get_version(symlink))


def test_binary_in_current_path(binary, application_ini, platform_ini, tmpdir):
    os.chdir(str(tmpdir))
    _check_version(get_version())


def test_with_ini_files_on_osx(binary, application_ini, platform_ini, monkeypatch, tmpdir):
    monkeypatch.setattr(sys, 'platform', 'darwin')
    # get_version is working with ini files next to the binary
    _check_version(get_version(binary=binary))

    # or if they are in the Resources dir
    # in this case the binary must be in a Contents dir, next
    # to the Resources dir
    contents_dir = tmpdir.mkdir('Contents')
    moved_binary = str(contents_dir.join(os.path.basename(binary)))
    shutil.move(binary, moved_binary)

    resources_dir = str(tmpdir.mkdir('Resources'))
    shutil.move(application_ini, resources_dir)
    shutil.move(platform_ini, resources_dir)

    _check_version(get_version(binary=moved_binary))


def test_invalid_binary_path(tmpdir):
    with pytest.raises(IOError):
        get_version(str(tmpdir.join('invalid')))


def test_without_ini_files(binary):
    """With missing ini files an exception should be thrown"""
    with pytest.raises(errors.AppNotFoundError):
        get_version(binary)


def test_without_platform_ini_file(binary, application_ini):
    """With a missing platform.ini file an exception should be thrown"""
    with pytest.raises(errors.AppNotFoundError):
        get_version(binary)


def test_without_application_ini_file(binary, platform_ini):
    """With a missing application.ini file an exception should be thrown"""
    with pytest.raises(errors.AppNotFoundError):
        get_version(binary)


def test_with_exe(application_ini, platform_ini, tmpdir):
    """Test that we can resolve .exe files"""
    binary = tmpdir.join('binary.exe')
    binary.write('foobar')
    _check_version(get_version(os.path.splitext(str(binary))[0]))


def test_not_found_with_binary_specified(binary):
    with pytest.raises(errors.LocalAppNotFoundError):
        get_version(binary)


def _check_version(version):
    assert version.get('application_id') == 'AppID'
    assert version.get('application_name') == 'AppName'
    assert version.get('application_display_name') == 'AppCodeName'
    assert version.get('application_version') == 'AppVersion'
    assert version.get('application_buildid') == 'AppBuildID'
    assert version.get('application_repository') == 'AppSourceRepo'
    assert version.get('application_changeset') == 'AppSourceStamp'
    assert version.get('application_vendor') == 'AppVendor'
    assert version.get('platform_name') is None
    assert version.get('platform_buildid') == 'PlatformBuildID'
    assert version.get('platform_repository') == 'PlatformSourceRepo'
    assert version.get('platform_changeset') == 'PlatformSourceStamp'
    assert version.get('invalid_key') is None
    assert version.get('platform_version') == 'PlatformMilestone'


if __name__ == '__main__':
    mozunit.main()
