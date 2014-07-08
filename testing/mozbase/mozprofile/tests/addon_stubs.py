#!/usr/bin/env python

import os
import tempfile
import zipfile

import mozfile


here = os.path.dirname(os.path.abspath(__file__))

# stubs is a dict of the form {'addon id': 'install manifest content'}
stubs = {
    'test-addon-1@mozilla.org': 'test_addon_1.rdf',
    'test-addon-2@mozilla.org': 'test_addon_2.rdf',
    'test-addon-3@mozilla.org': 'test_addon_3.rdf',
    'test-addon-4@mozilla.org': 'test_addon_4.rdf',
    'test-addon-invalid-no-id@mozilla.org': 'test_addon_invalid_no_id.rdf',
    'test-addon-invalid-version@mozilla.org': 'test_addon_invalid_version.rdf',
    'test-addon-invalid-no-manifest@mozilla.org': None,
    'test-addon-invalid-not-wellformed@mozilla.org': 'test_addon_invalid_not_wellformed.rdf',
    'test-addon-unpack@mozilla.org': 'test_addon_unpack.rdf'}


def generate_addon(addon_id, path=None, name=None, xpi=True):
    """
    Method to generate a single addon.

    :param addon_id: id of an addon to generate from the stubs dictionary
    :param path: path where addon and .xpi should be generated
    :param name: name for the addon folder or .xpi file
    :param xpi: Flag if an XPI or folder should be generated

    Returns the file-path of the addon's .xpi file
    """

    if not addon_id in stubs.keys():
        raise IOError('Requested addon stub "%s" does not exist' % addon_id)

    # Generate directory structure for addon
    try:
        tmpdir = path or tempfile.mkdtemp()
        addon_dir = os.path.join(tmpdir, name or addon_id)
        os.mkdir(addon_dir)
    except IOError:
        raise IOError('Could not generate directory structure for addon stub.')

    # Write install.rdf for addon
    if stubs[addon_id]:
        install_rdf = os.path.join(addon_dir, 'install.rdf')
        with open(install_rdf, 'w') as f:
            manifest = os.path.join(here, 'install_manifests', stubs[addon_id])
            f.write(open(manifest, 'r').read())

    if not xpi:
        return addon_dir

    # Generate the .xpi for the addon
    xpi_file = os.path.join(tmpdir, (name or addon_id) + '.xpi')
    with zipfile.ZipFile(xpi_file, 'w') as x:
        x.write(install_rdf, install_rdf[len(addon_dir):])

    # Ensure we remove the temporary folder to not install the addon twice
    mozfile.rmtree(addon_dir)

    return xpi_file


def generate_manifest(addon_list, path=None):
    tmpdir = path or tempfile.mkdtemp()
    addons = [generate_addon(addon, path=tmpdir) for addon in addon_list]

    manifest = os.path.join(tmpdir, 'manifest.ini')
    with open(manifest, 'w') as f:
        for addon in addons:
            f.write('[' + addon + ']\n')

    return manifest
