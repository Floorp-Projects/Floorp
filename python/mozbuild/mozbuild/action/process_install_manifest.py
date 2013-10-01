# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import argparse
import sys
from mozpack.copier import FileCopier
from mozpack.manifests import InstallManifest


COMPLETE = 'From {dest}: Kept {existing} existing; Added/updated {updated}; ' \
    'Removed {rm_files} files and {rm_dirs} directories.'


def process_manifest(destdir, paths, remove_unaccounted=True):
    manifest = InstallManifest()
    for path in paths:
        manifest |= InstallManifest(path=path)

    copier = FileCopier()
    manifest.populate_registry(copier)
    return copier.copy(destdir, remove_unaccounted=remove_unaccounted)


def main(argv):
    parser = argparse.ArgumentParser(
        description='Process install manifest files.')

    parser.add_argument('destdir', help='Destination directory.')
    parser.add_argument('manifests', nargs='+', help='Path to manifest file(s).')
    parser.add_argument('--no-remove', action='store_true',
        help='Do not remove unaccounted files from destination.')

    args = parser.parse_args(argv)

    result = process_manifest(args.destdir, args.manifests,
        remove_unaccounted=not args.no_remove)

    print(COMPLETE.format(dest=args.destdir,
        existing=result.existing_files_count,
        updated=result.updated_files_count,
        rm_files=result.removed_files_count,
        rm_dirs=result.removed_directories_count))

if __name__ == '__main__':
    main(sys.argv[1:])
