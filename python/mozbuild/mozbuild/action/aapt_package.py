#!/bin/python

# -*- Mode: python; indent-tabs-mode: nil; tab-width: 40 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Invoke Android `aapt package`.

Right now, this passes arguments through.  Eventually it will
implement a much restricted version of the Gradle build system's
resource merging algorithm before invoking aapt.
'''

from __future__ import (
    print_function,
    unicode_literals,
)

import argparse
import os
import subprocess
import sys

import buildconfig
import mozpack.path as mozpath

import merge_resources


def uniqify(iterable):
    """Remove duplicates from iterable, preserving order."""
    # Cribbed from
    # https://thingspython.wordpress.com/2011/03/09/snippet-uniquify-a-sequence-preserving-order/.
    seen = set()
    return [item for item in iterable if not (item in seen or seen.add(item))]


def main(*argv):
    parser = argparse.ArgumentParser(
        description='Invoke Android `aapt package`.')

    # These serve to order build targets; they're otherwise ignored.
    parser.add_argument('ignored_inputs', nargs='*')
    parser.add_argument('-f', action='store_true', default=False,
                        help='force overwrite of existing files')
    parser.add_argument('-F', required=True,
                        help='specify the apk file to output')
    parser.add_argument('-M', required=True,
                        help='specify full path to AndroidManifest.xml to include in zip')
    parser.add_argument('-J', required=True,
                        help='specify where to output R.java resource constant definitions')
    parser.add_argument('-S', action='append', dest='res_dirs',
                        default=[],
                        help='directory in which to find resources. ' +
                        'Multiple directories will be scanned and the first ' +
                        'match found (left to right) will take precedence.')
    parser.add_argument('-A', action='append', dest='assets_dirs',
                        default=[],
                        help='additional directory in which to find raw asset files')
    parser.add_argument('--extra-packages', action='append',
                        default=[],
                        help='generate R.java for libraries')
    parser.add_argument('--output-text-symbols', required=True,
                        help='Generates a text file containing the resource ' +
                             'symbols of the R class in the specified folder.')
    parser.add_argument('--verbose', action='store_true', default=False,
                        help='provide verbose output')

    args = parser.parse_args(argv)

    args.res_dirs = uniqify(args.res_dirs)
    args.assets_dirs = uniqify(args.assets_dirs)
    args.extra_packages = uniqify(args.extra_packages)

    import itertools

    debug = False
    if (not buildconfig.substs['MOZILLA_OFFICIAL']) or \
       (buildconfig.substs['NIGHTLY_BUILD'] and buildconfig.substs['MOZ_DEBUG']):
        debug = True

    merge_resources.main('merged', True, *args.res_dirs)

    cmd = [
        buildconfig.substs['AAPT'],
        'package',
    ] + \
    (['-f'] if args.f else []) + \
    [
        '-m',
        '-M', args.M,
        '-I', mozpath.join(buildconfig.substs['ANDROID_SDK'], 'android.jar'),
        '--auto-add-overlay',
    ] + \
    list(itertools.chain(*(('-A', x) for x in args.assets_dirs))) + \
    ['-S', os.path.abspath('merged')] + \
    (['--extra-packages', ':'.join(args.extra_packages)] if args.extra_packages else []) + \
    ['--custom-package', 'org.mozilla.gecko'] + \
    ['--no-version-vectors'] + \
    (['--debug-mode'] if debug else []) + \
    [
        '-F',
        args.F,
        '-J',
        args.J,
        '--output-text-symbols',
        args.output_text_symbols,
        '--ignore-assets',
        '!.svn:!.git:.*:<dir>_*:!CVS:!thumbs.db:!picasa.ini:!*.scc:*~:#*:*.rej:*.orig',
    ]

    # We run aapt to produce gecko.ap_ and gecko-nodeps.ap_; it's
    # helpful to tag logs with the file being produced.
    logtag = os.path.basename(args.F)

    if args.verbose:
        print('[aapt {}] {}'.format(logtag, ' '.join(cmd)))

    try:
        subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print('\n'.join(['[aapt {}] {}'.format(logtag, line) for line in e.output.splitlines()]))
        return 1


if __name__ == '__main__':
    sys.exit(main(*sys.argv[1:]))
