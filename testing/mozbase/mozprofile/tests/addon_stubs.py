#!/usr/bin/env python

import tempfile
import mozhttpd
import os
import zipfile


here = os.path.dirname(os.path.abspath(__file__))

# stubs is a dict of the form {'addon name': 'install manifest content'}
stubs = {
    'empty-0-1.xpi':
    open(os.path.join(here, "install_manifests", "empty-0-1.rdf"), 'r').read(),
    'empty-0-2.xpi':
    open(os.path.join(here, "install_manifests", "empty-0-2.rdf"), 'r').read(),
    'another-empty-0-1.xpi':
    open(os.path.join(here, "install_manifests", "another-empty-0-1.rdf"), 'r').read(),
    'empty-invalid.xpi':
    open(os.path.join(here, "install_manifests", "empty-invalid.rdf"), 'r').read()}

def generate_addon(name, path=None):
    """
    Method to generate a single addon.

    :param name: name of an addon to generate from the stubs dictionary
    :param path: path where addon and .xpi should be generated

    Returns the file-path of the addon's .xpi file
    """

    if name in stubs.keys():
        addon = name
    else:
        # If `name` is not in listed stubs, raise exception
        raise IOError('Requested addon stub does not exist')

    # Generate directory structure for addon
    try:
        if path:
            tmpdir = path
        else:
            tmpdir = tempfile.mkdtemp()
        addon_dir = os.path.join(tmpdir, addon[:-4])
        os.mkdir(addon_dir)
        install_rdf = os.path.join(addon_dir, 'install.rdf')
        xpi = os.path.join(tmpdir, addon)
    except IOError:
        raise IOError('Could not generate directory structure for addon stub.')

    # Write install.rdf for addon
    with open(install_rdf, 'w') as f:
        f.write(stubs[addon])
    # Generate the .xpi for the addon
    with zipfile.ZipFile(xpi, 'w') as x:
        x.write(install_rdf, install_rdf[len(addon_dir):])

    return xpi

def generate_invalid_addon(path=None):
    """
    Method to create an invalid addon

    Returns the file-path to the .xpi of an invalid addon
    """
    return generate_addon(name='empty-invalid.xpi', path=path)

def generate_manifest(path=None):

    if path:
        tmpdir = path
    else:
        tmpdir = tempfile.mkdtemp()

    addon_list = ['empty-0-1.xpi', 'another-empty-0-1.xpi']
    for a in addon_list:
        generate_addon(a, tmpdir)

    manifest = os.path.join(tmpdir, 'manifest.ini')
    with open(manifest, 'w') as f:
        for a in addon_list:
            f.write('[' + a + ']\n')

    return manifest
