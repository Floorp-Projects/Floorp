# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This action is used to produce test archives.
#
# Ideally, the data in this file should be defined in moz.build files.
# It is defined inline because this was easiest to make test archive
# generation faster.

from __future__ import absolute_import, print_function, unicode_literals

import argparse
import itertools
import os
import sys
import time

from mozpack.files import FileFinder
from mozpack.mozjar import JarWriter
import mozpack.path as mozpath

import buildconfig

STAGE = mozpath.join(buildconfig.topobjdir, 'dist', 'test-stage')


ARCHIVE_FILES = {
    'common': [
        {
            'source': STAGE,
            'base': '',
            'pattern': '**',
            'ignore': [
                'cppunittest/**',
                'mochitest/**',
                'reftest/**',
                'talos/**',
                'web-platform/**',
                'xpcshell/**',
            ],
        },
        {
            'source': buildconfig.topobjdir,
            'base': '_tests',
            'pattern': 'modules/**',
        },
        {
            'source': buildconfig.topobjdir,
            'base': '_tests',
            'pattern': 'mozbase/**',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'js/src',
            'pattern': 'jit-test/**',
            'dest': 'jit-test',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'js/src/tests',
            'pattern': 'ecma_6/**',
            'dest': 'jit-test/tests',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'js/src/tests',
            'pattern': 'js1_8_5/**',
            'dest': 'jit-test/tests',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'js/src/tests',
            'pattern': 'lib/**',
            'dest': 'jit-test/tests',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'js/src',
            'pattern': 'jsapi.h',
            'dest': 'jit-test',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'tps/**',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'services/sync/',
            'pattern': 'tps/**',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'services/sync/tests/tps',
            'pattern': '**',
            'dest': 'tps/tests',
        },
    ],
    'cppunittest': [
        {
            'source': STAGE,
            'base': '',
            'pattern': 'cppunittest/**',
        },
        # We don't ship these files if startup cache is disabled, which is
        # rare. But it shouldn't matter for test archives.
        {
            'source': buildconfig.topsrcdir,
            'base': 'startupcache/test',
            'pattern': 'TestStartupCacheTelemetry.*',
            'dest': 'cppunittest',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'runcppunittests.py',
            'dest': 'cppunittest',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'remotecppunittests.py',
            'dest': 'cppunittest',
        },
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'cppunittest.ini',
            'dest': 'cppunittest',
        },
        {
            'source': buildconfig.topobjdir,
            'base': '',
            'pattern': 'mozinfo.json',
            'dest': 'cppunittest',
        },
    ],
    'mochitest': [
        {
            'source': buildconfig.topobjdir,
            'base': '_tests/testing',
            'pattern': 'mochitest/**',
        },
        {
            'source': STAGE,
            'base': '',
            'pattern': 'mochitest/**',
        },
    ],
    'mozharness': [
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'mozharness/**',
        },
    ],
    'reftest': [
        {
            'source': buildconfig.topobjdir,
            'base': '_tests',
            'pattern': 'reftest/**',
        },
        {
            'source': buildconfig.topobjdir,
            'base': '',
            'pattern': 'mozinfo.json',
            'dest': 'reftest',
        },
    ],
    'talos': [
        {
            'source': buildconfig.topsrcdir,
            'base': 'testing',
            'pattern': 'talos/**',
        },
    ],
    'web-platform': [
        {
            'source': buildconfig.topobjdir,
            'base': '_tests',
            'pattern': 'web-platform/**',
        },
        {
            'source': buildconfig.topobjdir,
            'base': '',
            'pattern': 'mozinfo.json',
            'dest': 'web-platform',
        },
    ],
    'xpcshell': [
        {
            'source': buildconfig.topobjdir,
            'base': '_tests/xpcshell',
            'pattern': '**',
            'dest': 'xpcshell/tests',
        },
        {
            'source': STAGE,
            'base': '',
            'pattern': 'xpcshell/**',
        },
    ],
}


# "common" is our catch all archive and it ignores things from other archives.
# Verify nothing sneaks into ARCHIVE_FILES without a corresponding exclusion
# rule in the "common" archive.
for k, v in ARCHIVE_FILES.items():
    # Skip mozharness because it isn't staged.
    if k in ('common', 'mozharness'):
        continue

    ignores = set(itertools.chain(*(e.get('ignore', [])
                                  for e in ARCHIVE_FILES['common'])))

    if not any(p.startswith('%s/' % k) for p in ignores):
        raise Exception('"common" ignore list probably should contain %s' % k)


def find_files(archive):
    for entry in ARCHIVE_FILES[archive]:
        source = entry['source']
        base = entry.get('base', '')
        pattern = entry.get('pattern')
        dest = entry.get('dest')
        ignore = list(entry.get('ignore', []))
        ignore.append('**/.mkdir.done')
        ignore.append('**/*.pyc')

        common_kwargs = {
            'find_executables': False,
            'find_dotfiles': True,
            'ignore': ignore,
        }

        finder = FileFinder(os.path.join(source, base), **common_kwargs)

        for p, f in finder.find(pattern):
            if dest:
                p = mozpath.join(dest, p)
            yield p, f


def find_reftest_dirs(topsrcdir, manifests):
    from reftest import ReftestManifest

    dirs = set()
    for p in manifests:
        m = ReftestManifest()
        m.load(os.path.join(topsrcdir, p))
        dirs |= m.dirs

    dirs = {mozpath.normpath(d[len(topsrcdir):]).lstrip('/') for d in dirs}

    # Filter out children captured by parent directories because duplicates
    # will confuse things later on.
    def parents(p):
        while True:
            p = mozpath.dirname(p)
            if not p:
                break
            yield p

    seen = set()
    for d in sorted(dirs, key=len):
        if not any(p in seen for p in parents(d)):
            seen.add(d)

    return sorted(seen)


def insert_reftest_entries(entries):
    """Reftests have their own mechanism for defining tests and locations.

    This function is called when processing the reftest archive to process
    reftest test manifests and insert the results into the existing list of
    archive entries.
    """
    manifests = (
        'layout/reftests/reftest.list',
        'testing/crashtest/crashtests.list',
    )

    for base in find_reftest_dirs(buildconfig.topsrcdir, manifests):
        entries.append({
            'source': buildconfig.topsrcdir,
            'base': '',
            'pattern': '%s/**' % base,
            'dest': 'reftest/tests',
        })


def main(argv):
    parser = argparse.ArgumentParser(
        description='Produce test archives')
    parser.add_argument('archive', help='Which archive to generate')
    parser.add_argument('outputfile', help='File to write output to')

    args = parser.parse_args(argv)

    if not args.outputfile.endswith('.zip'):
        raise Exception('expected zip output file')

    # Adjust reftest entries only if processing reftests (because it is
    # unnecessary overhead otherwise).
    if args.archive == 'reftest':
        insert_reftest_entries(ARCHIVE_FILES['reftest'])

    file_count = 0
    t_start = time.time()
    with open(args.outputfile, 'wb') as fh:
        # Experimentation revealed that level 5 is significantly faster and has
        # marginally larger sizes than higher values and is the sweet spot
        # for optimal compression. Read the detailed commit message that
        # introduced this for raw numbers.
        with JarWriter(fileobj=fh, optimize=False, compress_level=5) as writer:
            res = find_files(args.archive)
            for p, f in res:
                file_count += 1
                writer.add(p.encode('utf-8'), f.read(), mode=f.mode)

    duration = time.time() - t_start
    zip_size = os.path.getsize(args.outputfile)
    basename = os.path.basename(args.outputfile)
    print('Wrote %d files in %d bytes to %s in %.2fs' % (
          file_count, zip_size, basename, duration))


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
