# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import json
import os.path
import sys

import buildconfig
import mozpack.path as mozpath

from mozpack.copier import FileRegistry
from mozpack.manifests import InstallManifest


# A list of build manifests, and their relative base paths, from which to
# extract lists of install files. These vary depending on which backend we're
# using, so nonexistent manifests are ignored.
manifest_paths = (
    ('', '_build_manifests/install/dist_bin'),
    ('', 'faster/install_dist_bin'),
    ('browser', 'faster/install_dist_bin_browser'),
)


def get_registry(paths):
    used_paths = set()

    registry = FileRegistry()
    for base, path in paths:
        full_path = mozpath.join(buildconfig.topobjdir, path)
        if not os.path.exists(full_path):
            continue

        used_paths.add(full_path)

        reg = FileRegistry()
        InstallManifest(full_path).populate_registry(reg)

        for p, f in reg:
            path = mozpath.join(base, p)
            try:
                registry.add(path, f)
            except Exception:
                pass

    return registry, used_paths


def get_child(base, path):
    """Returns the nearest parent of `path` which is an immediate child of
    `base`"""

    dirname = mozpath.dirname(path)
    while dirname != base:
        path = dirname
        dirname = mozpath.dirname(path)
    return path


def main(output, *args):
    parser = argparse.ArgumentParser(
        description='Produces a JSON manifest of built-in add-ons')
    parser.add_argument('--features', type=str, dest='featuresdir',
                        action='store', help=('The distribution sub-directory '
                                              'containing feature add-ons'))
    args = parser.parse_args(args)

    registry, inputs = get_registry(manifest_paths)

    dicts = {}
    for path in registry.match('dictionaries/*.dic'):
        base, ext = os.path.splitext(mozpath.basename(path))
        dicts[base] = path

    listing = {
        "dictionaries": dicts,
    }

    if args.featuresdir:
        features = set()
        for p in registry.match('%s/*' % args.featuresdir):
            features.add(mozpath.basename(get_child(args.featuresdir, p)))

        listing["system"] = sorted(features)

    json.dump(listing, output, sort_keys=True)

    return inputs


if __name__ == '__main__':
    main(sys.stdout, *sys.argv[1:])
