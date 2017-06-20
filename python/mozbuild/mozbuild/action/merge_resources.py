#!/bin/python

# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
A hacked together clone of the Android Gradle plugin's resource
merging algorithm.  To be abandoned in favour of --with-gradle as soon
as possible!
'''

from __future__ import (
    print_function,
    unicode_literals,
)

from collections import defaultdict
import re
import os
import sys

from mozbuild.util import ensureParentDir
from mozpack.copier import (
    FileCopier,
)
from mozpack.manifests import (
    InstallManifest,
)
import mozpack.path as mozpath
from mozpack.files import (
    FileFinder,
)

import xml.etree.cElementTree as ET


# From https://github.com/miracle2k/android-platform_sdk/blob/master/common/src/com/android/resources/ResourceType.java.
# TODO: find a more authoritative source!
resource_type = {
    "anim": 0,
    "animator": 1,
    # The only interesting ones.
    "string-array": 2,
    "integer-array": 2,
    "attr": 3,
    "bool": 4,
    "color": 5,
    "declare-styleable": 6,
    "dimen": 7,
    "drawable": 8,
    "fraction": 9,
    "id": 10,
    "integer": 11,
    "interpolator": 12,
    "layout": 13,
    "menu": 14,
    "mipmap": 15,
    "plurals": 16,
    "raw": 17,
    "string": 18,
    "style": 19,
    "styleable": 20,
    "xml": 21,
    # "public": 0,
}


def uniqify(iterable):
    """Remove duplicates from iterable, preserving order."""
    # Cribbed from https://thingspython.wordpress.com/2011/03/09/snippet-uniquify-a-sequence-preserving-order/.
    seen = set()
    return [item for item in iterable if not (item in seen or seen.add(item))]


# Exclusions, arising in appcompat-v7-23.4.0.
MANIFEST_EXCLUSIONS = (
    'color/abc_background_cache_hint_selector_material_dark.xml',
    'color/abc_background_cache_hint_selector_material_light.xml',
)

SMALLEST_SCREEN_WIDTH_QUALIFIER_RE = re.compile(r"(^|-)w(\d+)dp($|-)")
SCREEN_WIDTH_QUALIFIER_RE = re.compile(r"(^|-)sw(\d+)dp($|-)")
# Different densities were introduced in different Android versions.
# However, earlier versions of aapt (like the one we are building
# with) don't have fine-grained versions; they're all lumped into v4.
DENSITIES = [
    (re.compile(r"(^|-)xxxhdpi($|-)"), 18),
    (re.compile(r"(^|-)560dpi($|-)"),  1),
    (re.compile(r"(^|-)xxhdpi($|-)"),  16),
    (re.compile(r"(^|-)400dpi($|-)"),  1),
    (re.compile(r"(^|-)360dpi($|-)"),  23),
    (re.compile(r"(^|-)xhdpi($|-)"),   8),
    (re.compile(r"(^|-)280dpi($|-)"),  22),
    (re.compile(r"(^|-)hdpi($|-)"),    4),
    (re.compile(r"(^|-)tvdpi($|-)"),   13),
    (re.compile(r"(^|-)mdpi($|-)"),    4),
    (re.compile(r"(^|-)ldpi($|-)"),    4),
    (re.compile(r"(^|-)anydpi($|-)"),  21),
    (re.compile(r"(^|-)nodpi($|-)"),   4),
]
SCREEN_SIZE_RE = re.compile(r"(^|-)(small|normal|large|xlarge)($|-)")

def with_version(dir):
    """Resources directories without versions (like values-large) that
correspond to resource filters added to Android in vN (like large,
which was added in v4) automatically get a -vN added (so values-large
becomes values-large-v4, since Android versions before v4 will not
recognize values-large)."""
    # Order matters!  We need to check for later features before
    # earlier features, so that "ldrtl-sw-large" will be v17, not v13
    # or v4.
    if '-ldrtl' in dir and '-v' not in dir:
        return '{}-v17'.format(dir)

    if re.search(SMALLEST_SCREEN_WIDTH_QUALIFIER_RE, dir) and '-v' not in dir:
        return '{}-v13'.format(dir)

    if re.search(SCREEN_WIDTH_QUALIFIER_RE, dir) and '-v' not in dir:
        return '{}-v13'.format(dir)

    for (density, _since) in DENSITIES:
        if re.search(density, dir) and '-v' not in dir:
            return '{}-v{}'.format(dir, 4)

    if re.search(SCREEN_SIZE_RE, dir) and '-v' not in dir:
        return '{}-v4'.format(dir)

    return dir


def classify(path):
    """Return `(resource, version)` for a given path.

`resource` is of the form `unversioned/name` where `unversionsed` is a resource
type (like "drawable" or "strings"), and `version` is an
integer version number or `None`."""
    dir, name = path.split('/')
    segments = dir.split('-')
    version = None
    for segment in segments[1:]:
        if segment.startswith('v'):
            version = int(segment[1:])
            break
    segments = [segment for segment in segments if not segment.startswith('v')]
    resource = '{}/{}'.format('-'.join(segments), name)
    return (resource, version)


def main(output_dirname, verbose, *input_dirs):
    # Map directories to source paths, like
    # `{'values-large-v11': ['/path/to/values-large-v11/strings.xml',
    #                        '/path/to/values-large-v11/colors.xml', ...], ...}`.
    values = defaultdict(list)
    # Map unversioned resource names to maps from versions to source paths, like:
    # `{'drawable-large/icon.png':
    #     {None: '/path/to/drawable-large/icon.png',
    #      11: '/path/to/drawable-large-v11/icon.png', ...}, ...}`.
    resources = defaultdict(dict)

    manifest = InstallManifest()

    for p in uniqify(input_dirs):
        finder = FileFinder(p, find_executables=False)

        values_pattern = 'values*/*.xml'
        for path, _ in finder.find('*/*'):
            if path in MANIFEST_EXCLUSIONS:
                continue

            source_path = mozpath.join(finder.base, path)

            if mozpath.match(path, values_pattern):
                dir, _name = path.split('/')
                dir = with_version(dir)
                values[dir].append(source_path)
                continue

            (resource, version) = classify(path)

            # Earlier paths are taken in preference to later paths.
            # This agrees with aapt.
            if version not in resources:
                resources[resource][version] = source_path

    # Step 1: merge all XML values into one single, sorted
    # per-configuration values.xml file.  This apes what the Android
    # Gradle resource merging algorithm does.
    merged_values = defaultdict(list)

    for dir, files in values.items():
        for file in files:
            values = ET.ElementTree(file=file).getroot()
            merged_values[dir].extend(values)

        values = ET.Element('resources')
        # Sort by <type> tag, and then by name.  Note that <item
        # type="type"> is equivalent to <type>.
        key = lambda x: (resource_type.get(x.get('type', x.tag)), x.get('name'))
        values[:] = sorted(merged_values[dir], key=key)

        for value in values:
            if value.get('name') == 'TextAppearance.Design.Snackbar.Message':
                if value.get('{http://schemas.android.com/tools}override', False):
                    values.remove(value)
                    break

        merged_values[dir] = values

    for dir, values in merged_values.items():
        o = mozpath.join(output_dirname, dir, '{}.xml'.format(dir))
        ensureParentDir(o)
        ET.ElementTree(values).write(o)

        manifest.add_required_exists(mozpath.join(dir, '{}.xml'.format(dir)))

    # Step 2a: add version numbers for unversioned features
    # corresponding to when the feature was introduced.  Resource
    # qualifiers will never be recognized by Android versions before
    # they were introduced.  For example, density qualifiers are
    # supported only in Android v4 and above.  Therefore
    # "drawable-hdpi" is implicitly "drawable-hdpi-v4".  We version
    # such unversioned resources here.
    for (resource, versions) in resources.items():
        if None in versions:
            dir, name = resource.split('/')
            new_dir = with_version(dir)
            (new_resource, new_version) = classify('{}/{}'.format(new_dir, name))
            if new_resource != resource:
                raise ValueError('this is bad')

            # `new_version` might be None: for example, `dir` might be "drawable".
            source_path = versions.pop(None)
            versions[new_version] = source_path

            if verbose:
                if new_version:
                    print("Versioning unversioned resource {} as {}-v{}/{}".format(source_path, dir, new_version, name))

    # TODO: make this a command line argument that takes MOZ_ANDROID_MIN_SDK_VERSION.
    min_sdk = 15
    retained = defaultdict(dict)

    # Step 2b: drop resource directories that will never be used by
    # Android on device.  This depends on the minimum supported
    # Android SDK version.  Suppose the minimum SDK is 15 and we have
    # drawable-v4/icon.png and drawable-v11/icon.png.  The v4 version
    # will never be chosen, since v15 is always greater than v11.
    for (resource, versions) in resources.items():
        def key(v):
            return 0 if v is None else v
        # Versions in descending order.
        version_list = sorted(versions.keys(), key=key, reverse=True)
        for version in version_list:
            retained[resource][version] = versions[version]
            if version is not None and version <= min_sdk:
                break

    if set(retained.keys()) != set(resources.keys()):
        raise ValueError('Something terrible has happened; retained '
                         'resource names do not match input resources '
                         'names')

    if verbose:
        for resource in resources:
            if resources[resource] != retained[resource]:
                for version in sorted(resources[resource].keys(), reverse=True):
                    if version in retained[resource]:
                        print("Keeping reachable resource {}".format(resources[resource][version]))
                    else:
                        print("Dropping unreachable resource {}".format(resources[resource][version]))

    # Populate manifest.
    for (resource, versions) in retained.items():
        for version in sorted(versions.keys(), reverse=True):
            path = resource
            if version:
                dir, name = resource.split('/')
                path = '{}-v{}/{}'.format(dir, version, name)
            manifest.add_copy(versions[version], path)


    copier = FileCopier()
    manifest.populate_registry(copier)
    print('mr', os.getcwd())
    result = copier.copy(output_dirname,
                         remove_unaccounted=True,
                         remove_all_directory_symlinks=False,
                         remove_empty_directories=True)

    if verbose:
        print('Updated:', result.updated_files_count)
        print('Removed:', result.removed_files_count + result.removed_directories_count)
        print('Existed:', result.existing_files_count)

    return 0


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
