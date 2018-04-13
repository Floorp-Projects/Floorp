#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import os
import zipfile

import mozunit
import pytest

import mozprofile
from addon_stubs import generate_addon

here = os.path.dirname(os.path.abspath(__file__))


@pytest.fixture
def profile():
    return mozprofile.Profile()


@pytest.fixture
def am(profile):
    return profile.addons


def test_install_multiple_same_source(tmpdir, am):
    path = tmpdir.strpath

    # Generate installer stubs for all possible types of addons
    addon_xpi = generate_addon('test-addon-1@mozilla.org',
                               path=path)
    addon_folder = generate_addon('test-addon-1@mozilla.org',
                                  path=path,
                                  xpi=False)

    # The same folder should not be installed twice
    am.install([addon_folder, addon_folder])
    assert am.installed_addons == [addon_folder]
    am.clean()

    # The same XPI file should not be installed twice
    am.install([addon_xpi, addon_xpi])
    assert am.installed_addons == [addon_xpi]
    am.clean()

    # Even if it is the same id the add-on should be installed twice, if
    # specified via XPI and folder
    am.install([addon_folder, addon_xpi])
    assert len(am.installed_addons) == 2
    assert addon_folder in am.installed_addons
    assert addon_xpi in am.installed_addons
    am.clean()


def test_install_webextension_from_dir(tmpdir, am):
    tmpdir = tmpdir.strpath

    addon = os.path.join(here, 'addons', 'apply-css.xpi')
    zipped = zipfile.ZipFile(addon)
    try:
        zipped.extractall(tmpdir)
    finally:
        zipped.close()
    am.install(tmpdir)
    assert len(am.installed_addons) == 1
    assert os.path.isdir(am.installed_addons[0])


def test_install_webextension(am):
    addon = os.path.join(here, 'addons', 'apply-css.xpi')

    am.install(addon)
    assert len(am.installed_addons) == 1
    assert os.path.isfile(am.installed_addons[0])
    assert 'apply-css.xpi' == os.path.basename(am.installed_addons[0])

    details = am.addon_details(am.installed_addons[0])
    assert 'test-webext@quality.mozilla.org' == details['id']


def test_install_webextension_sans_id(am):
    addon = os.path.join(here, 'addons', 'apply-css-sans-id.xpi')
    am.install(addon)

    assert len(am.installed_addons) == 1
    assert os.path.isfile(am.installed_addons[0])
    assert 'apply-css-sans-id.xpi' == os.path.basename(am.installed_addons[0])

    details = am.addon_details(am.installed_addons[0])
    assert '@temporary-addon' in details['id']


def test_install_xpi(tmpdir, am):
    tmpdir = tmpdir.strpath

    addons_to_install = []
    addons_installed = []

    # Generate installer stubs and install them
    for ext in ['test-addon-1@mozilla.org', 'test-addon-2@mozilla.org']:
        temp_addon = generate_addon(ext, path=tmpdir)
        addons_to_install.append(am.addon_details(temp_addon)['id'])
        am.install(temp_addon)

    # Generate a list of addons installed in the profile
    addons_installed = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                        am.profile, 'extensions'))]
    assert addons_to_install.sort() == addons_installed.sort()


def test_install_folder(tmpdir, am):
    tmpdir = tmpdir.strpath

    # Generate installer stubs for all possible types of addons
    addons = []
    addons.append(generate_addon('test-addon-1@mozilla.org',
                                 path=tmpdir))
    addons.append(generate_addon('test-addon-2@mozilla.org',
                                 path=tmpdir,
                                 xpi=False))
    addons.append(generate_addon('test-addon-3@mozilla.org',
                                 path=tmpdir,
                                 name='addon-3'))
    addons.append(generate_addon('test-addon-4@mozilla.org',
                                 path=tmpdir,
                                 name='addon-4',
                                 xpi=False))
    addons.sort()

    am.install(tmpdir)

    assert am.installed_addons == addons


def test_install_unpack(tmpdir, am):
    tmpdir = tmpdir.strpath

    # Generate installer stubs for all possible types of addons
    addon_xpi = generate_addon('test-addon-unpack@mozilla.org',
                               path=tmpdir)
    addon_folder = generate_addon('test-addon-unpack@mozilla.org',
                                  path=tmpdir,
                                  xpi=False)
    addon_no_unpack = generate_addon('test-addon-1@mozilla.org',
                                     path=tmpdir)

    # Test unpack flag for add-on as XPI
    am.install(addon_xpi)
    assert am.installed_addons == [addon_xpi]
    am.clean()

    # Test unpack flag for add-on as folder
    am.install(addon_folder)
    assert am.installed_addons == [addon_folder]
    am.clean()

    # Test forcing unpack an add-on
    am.install(addon_no_unpack, unpack=True)
    assert am.installed_addons == [addon_no_unpack]
    am.clean()


def test_install_after_reset(tmpdir, profile):
    tmpdir = tmpdir.strpath
    am = profile.addons

    # Installing the same add-on after a reset should not cause a failure
    addon = generate_addon('test-addon-1@mozilla.org',
                           path=tmpdir, xpi=False)

    # We cannot use am because profile.reset() creates a new instance
    am.install(addon)
    profile.reset()

    am.install(addon)
    assert am.installed_addons == [addon]


def test_install_backup(tmpdir, am):
    tmpdir = tmpdir.strpath

    staged_path = os.path.join(am.profile, 'extensions')

    # Generate installer stubs for all possible types of addons
    addon_xpi = generate_addon('test-addon-1@mozilla.org',
                               path=tmpdir)
    addon_folder = generate_addon('test-addon-1@mozilla.org',
                                  path=tmpdir,
                                  xpi=False)
    addon_name = generate_addon('test-addon-1@mozilla.org',
                                path=tmpdir,
                                name='test-addon-1-dupe@mozilla.org')

    # Test backup of xpi files
    am.install(addon_xpi)
    assert am.backup_dir is None

    am.install(addon_xpi)
    assert am.backup_dir is not None
    assert os.listdir(am.backup_dir) == ['test-addon-1@mozilla.org.xpi']

    am.clean()
    assert os.listdir(staged_path) == ['test-addon-1@mozilla.org.xpi']
    am.clean()

    # Test backup of folders
    am.install(addon_folder)
    assert am.backup_dir is None

    am.install(addon_folder)
    assert am.backup_dir is not None
    assert os.listdir(am.backup_dir) == ['test-addon-1@mozilla.org']

    am.clean()
    assert os.listdir(staged_path) == ['test-addon-1@mozilla.org']
    am.clean()

    # Test backup of xpi files with another file name
    am.install(addon_name)
    assert am.backup_dir is None

    am.install(addon_xpi)
    assert am.backup_dir is not None
    assert os.listdir(am.backup_dir) == ['test-addon-1@mozilla.org.xpi']

    am.clean()
    assert os.listdir(staged_path) == ['test-addon-1@mozilla.org.xpi']
    am.clean()


def test_install_invalid_addons(tmpdir, am):
    tmpdir = tmpdir.strpath

    # Generate installer stubs for all possible types of addons
    addons = []
    addons.append(generate_addon('test-addon-invalid-no-manifest@mozilla.org',
                                 path=tmpdir,
                                 xpi=False))
    addons.append(generate_addon('test-addon-invalid-no-id@mozilla.org',
                                 path=tmpdir))

    am.install(tmpdir)

    assert am.installed_addons == []


@pytest.mark.xfail(reason="feature not implemented as part of AddonManger")
def test_install_error(am):
    """ Check install raises an error with an invalid addon"""
    temp_addon = generate_addon('test-addon-invalid-version@mozilla.org')
    # This should raise an error here
    with pytest.raises(Exception):
        am.install(temp_addon)


def test_addon_details(tmpdir, am):
    tmpdir = tmpdir.strpath

    # Generate installer stubs for a valid and invalid add-on manifest
    valid_addon = generate_addon('test-addon-1@mozilla.org',
                                 path=tmpdir)
    invalid_addon = generate_addon('test-addon-invalid-not-wellformed@mozilla.org',
                                   path=tmpdir)

    # Check valid add-on
    details = am.addon_details(valid_addon)
    assert details['id'] == 'test-addon-1@mozilla.org'
    assert details['name'] == 'Test Add-on 1'
    assert not details['unpack']
    assert details['version'] == '0.1'

    # Check invalid add-on
    with pytest.raises(mozprofile.addons.AddonFormatError):
        am.addon_details(invalid_addon)

    # Check invalid path
    with pytest.raises(IOError):
        am.addon_details('')

    # Check invalid add-on format
    addon_path = os.path.join(os.path.join(here, 'files'), 'not_an_addon.txt')
    with pytest.raises(mozprofile.addons.AddonFormatError):
        am.addon_details(addon_path)


def test_clean_addons(am):
    addon_one = generate_addon('test-addon-1@mozilla.org')
    addon_two = generate_addon('test-addon-2@mozilla.org')

    am.install(addon_one)
    installed_addons = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                        am.profile, 'extensions'))]

    # Create a new profile based on an existing profile
    # Install an extra addon in the new profile
    # Cleanup addons
    duplicate_profile = mozprofile.profile.Profile(profile=am.profile,
                                                   addons=addon_two)
    duplicate_profile.addons.clean()

    addons_after_cleanup = [str(x[:-len('.xpi')]) for x in os.listdir(os.path.join(
                            duplicate_profile.profile, 'extensions'))]
    # New addons installed should be removed by clean_addons()
    assert installed_addons == addons_after_cleanup


def test_noclean(tmpdir):
    """test `restore=True/False` functionality"""
    profile = tmpdir.mkdtemp().strpath
    tmpdir = tmpdir.mkdtemp().strpath

    # empty initially
    assert not bool(os.listdir(profile))

    # make an addon
    addons = [
        generate_addon('test-addon-1@mozilla.org', path=tmpdir),
        os.path.join(here, 'addons', 'empty.xpi'),
    ]

    # install it with a restore=True AddonManager
    am = mozprofile.addons.AddonManager(profile, restore=True)

    for addon in addons:
        am.install(addon)

    # now its there
    assert os.listdir(profile) == ['extensions']
    staging_folder = os.path.join(profile, 'extensions')
    assert os.path.exists(staging_folder)
    assert len(os.listdir(staging_folder)) == 2

    del am

    assert os.listdir(profile) == ['extensions']
    assert os.path.exists(staging_folder)
    assert os.listdir(staging_folder) == []


def test_remove_addon(tmpdir, am):
    tmpdir = tmpdir.strpath

    addons = []
    addons.append(generate_addon('test-addon-1@mozilla.org',
                                 path=tmpdir))
    addons.append(generate_addon('test-addon-2@mozilla.org',
                                 path=tmpdir))

    am.install(tmpdir)

    extensions_path = os.path.join(am.profile, 'extensions')
    staging_path = os.path.join(extensions_path)

    for addon in am._addons:
        am.remove_addon(addon)

    assert os.listdir(staging_path) == []
    assert os.listdir(extensions_path) == []


if __name__ == '__main__':
    mozunit.main()
