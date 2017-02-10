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

def dump_symbols(target, tracking_file):
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

    dump_syms_bin = os.path.join(buildconfig.topobjdir,
                                 'dist', 'host',
                                 'bin', 'dump_syms')
    dump_syms_bin = '%s%s' % (dump_syms_bin, buildconfig.substs['BIN_SUFFIX'])

    os_arch = buildconfig.substs['OS_ARCH']
    if os_arch == 'WINNT':
        sym_store_args.extend(['-c', '--vcs-info'])
        if os.environ.get('PDBSTR_PATH'):
            sym_store_args.append('-i')
    elif os_arch == 'Darwin':
        sym_store_args.extend(['-c', '-a', buildconfig.substs['OS_TEST'], '--vcs-info'])
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

    args = ([buildconfig.substs['PYTHON'], os.path.join(buildconfig.topsrcdir, 'toolkit',
                                                       'crashreporter', 'tools', 'symbolstore.py')] +
            sym_store_args +
            ['-s', buildconfig.topsrcdir, dump_syms_bin, os.path.join(buildconfig.topobjdir,
                                                                      'dist',
                                                                      'crashreporter-symbols'),
             os.path.abspath(target)])
    print('Running: %s' % ' '.join(args))
    out_files = subprocess.check_output(args)
    with open(tracking_file, 'w') as fh:
        fh.write(out_files)
        fh.flush()

def main(argv):
    if len(argv) != 2:
        print("Usage: dumpsymbols.py <library or program> <tracking file>",
              file=sys.stderr)
        return 1

    return dump_symbols(*argv)


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
