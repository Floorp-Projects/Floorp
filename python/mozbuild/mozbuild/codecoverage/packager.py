# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import buildconfig
import errno
import json
import sys

from mozpack.copier import Jarrer, FileRegistry
from mozpack.files import FileFinder, GeneratedFile
from mozpack.manifests import (
    InstallManifest,
    UnreadableInstallManifest,
)
import mozpack.path as mozpath


def describe_install_manifest(manifest, dest_dir):
    try:
        manifest = InstallManifest(manifest)
    except UnreadableInstallManifest:
        raise IOError(errno.EINVAL, "Error parsing manifest file", manifest)

    reg = FileRegistry()

    mapping = {}
    manifest.populate_registry(reg)
    dest_dir = mozpath.join(buildconfig.topobjdir, dest_dir)
    for dest_file, src in reg:
        if hasattr(src, "path"):
            dest_path = mozpath.join(dest_dir, dest_file)
            relsrc_path = mozpath.relpath(src.path, buildconfig.topsrcdir)
            mapping[dest_path] = relsrc_path

    return mapping


def package_coverage_data(root, output_file):
    finder = FileFinder(root)
    jarrer = Jarrer()
    for p, f in finder.find("**/*.gcno"):
        jarrer.add(p, f)

    dist_include_manifest = mozpath.join(
        buildconfig.topobjdir, "_build_manifests", "install", "dist_include"
    )
    linked_files = describe_install_manifest(dist_include_manifest, "dist/include")
    mapping_file = GeneratedFile(json.dumps(linked_files, sort_keys=True))
    jarrer.add("linked-files-map.json", mapping_file)
    jarrer.copy(output_file)


def cli(args=sys.argv[1:]):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-o", "--output-file", dest="output_file", help="Path to save packaged data to."
    )
    parser.add_argument(
        "--root", dest="root", default=None, help="Root directory to search from."
    )
    args = parser.parse_args(args)

    if not args.root:
        from buildconfig import topobjdir

        args.root = topobjdir

    return package_coverage_data(args.root, args.output_file)


if __name__ == "__main__":
    sys.exit(cli())
