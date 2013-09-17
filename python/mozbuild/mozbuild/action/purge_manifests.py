# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script is used to purge a directory of unwanted files as defined by
# a manifest file.

from __future__ import print_function, unicode_literals

import argparse
import os
import sys
import threading

from mozpack.manifests import PurgeManifest


def do_purge(purger, dest, state):
    state['result'] = purger.purge(dest)


def process_manifest(topdir, manifest_path):
    manifest = PurgeManifest(path=manifest_path)
    purger = manifest.get_purger()
    full = os.path.join(topdir, manifest.relpath)

    state = dict(
        relpath=manifest.relpath,
        result=None,
    )

    t = threading.Thread(target=do_purge, args=(purger, full, state))
    state['thread'] = t
    t.start()

    return state


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Purge a directory of untracked files.')

    parser.add_argument('--directory', '-d',
        help='Directory containing manifest files. Will process every file '
            'in directory.')
    parser.add_argument('topdir',
        help='Top directory all paths are evaluated from.')
    parser.add_argument('manifests', nargs='*',
        help='List of manifest files defining purge operations to perform.')

    args = parser.parse_args()

    states = []

    print('Purging unaccounted files from object directory...')

    # We perform purging using threads for performance reasons. Hopefully
    # multiple I/O operations will be faster than just 1.
    paths = []
    if args.directory:
        for path in sorted(os.listdir(args.directory)):
            paths.append(os.path.join(args.directory, path))

    paths.extend(args.manifests)

    for path in paths:
        states.append(process_manifest(args.topdir, path))

    for state in states:
        state['thread'].join()
        print('Deleted %d files and %d directories from %s.' % (
            state['result'].removed_files_count,
            state['result'].removed_directories_count,
            state['relpath']
        ))

    print('Finished purging.')

    sys.exit(0)
