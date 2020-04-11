# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

import argparse
import buildconfig
import subprocess
import shutil
import sys
import os


def dump_symbols(target, tracking_file, count_ctors=False):
    # Our tracking file, if present, will contain path(s) to the previously generated
    # symbols. Remove them in this case so we don't simply accumulate old symbols
    # during incremental builds.
    if os.path.isfile(os.path.normpath(tracking_file)):
        with open(tracking_file, 'r') as fh:
            files = fh.read().splitlines()
        dirs = set(os.path.dirname(f) for f in files)
        for d in dirs:
            shutil.rmtree(os.path.join(buildconfig.topobjdir, 'dist',
                                       'crashreporter-symbols', d),
                          ignore_errors=True)

    # Build default args for symbolstore.py based on platform.
    sym_store_args = []

    # Find the `dump_syms` binary to use.
    dump_syms_bin = None
    dump_syms_binaries = []

    default_bin = buildconfig.substs.get('DUMP_SYMS')
    if default_bin:
        dump_syms_binaries.append(default_bin)

    # Fallback to the in-tree breakpad version.
    dump_syms_binaries.append(
        os.path.join(buildconfig.topobjdir, 'dist', 'host', 'bin',
                     'dump_syms' + buildconfig.substs['BIN_SUFFIX']))

    for dump_syms_bin in dump_syms_binaries:
        if os.path.exists(dump_syms_bin):
            break

    os_arch = buildconfig.substs['OS_ARCH']
    if os_arch == 'WINNT':
        sym_store_args.extend(['-c', '--vcs-info'])
        if 'PDBSTR' in buildconfig.substs:
            sym_store_args.append('-i')
    elif os_arch == 'Darwin':
        cpu = {
            'x86': 'i386',
        }.get(buildconfig.substs['TARGET_CPU'], buildconfig.substs['TARGET_CPU'])
        sym_store_args.extend(['-c', '-a', cpu, '--vcs-info'])
    elif os_arch == 'Linux':
        sym_store_args.extend(['-c', '--vcs-info'])

    sym_store_args.append('--install-manifest=%s,%s' % (os.path.join(buildconfig.topobjdir,
                                                                     '_build_manifests',
                                                                     'install',
                                                                     'dist_include'),
                                                        os.path.join(buildconfig.topobjdir,
                                                                     'dist',
                                                                     'include')))
    objcopy = buildconfig.substs.get('OBJCOPY')
    if objcopy:
        os.environ['OBJCOPY'] = objcopy

    args = ([buildconfig.substs['PYTHON'],
             os.path.join(buildconfig.topsrcdir, 'toolkit',
                          'crashreporter', 'tools', 'symbolstore.py')] +
            sym_store_args +
            ['-s', buildconfig.topsrcdir, dump_syms_bin, os.path.join(buildconfig.topobjdir,
                                                                      'dist',
                                                                      'crashreporter-symbols'),
             os.path.abspath(target)])
    if count_ctors:
        args.append('--count-ctors')
    print('Running: %s' % ' '.join(args))
    out_files = subprocess.check_output(args, universal_newlines=True)
    with open(tracking_file, 'w', encoding='utf-8', newline='\n') as fh:
        fh.write(out_files)
        fh.flush()


def main(argv):
    parser = argparse.ArgumentParser(
        usage="Usage: dumpsymbols.py <library or program> <tracking file>")
    parser.add_argument("--count-ctors",
                        action="store_true", default=False,
                        help="Count static initializers")
    parser.add_argument("library_or_program",
                        help="Path to library or program")
    parser.add_argument("tracking_file",
                        help="Tracking file")
    args = parser.parse_args()

    return dump_symbols(args.library_or_program, args.tracking_file,
                        args.count_ctors)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
