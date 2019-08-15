#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import sys
import os
import shutil
import subprocess
import tempfile
import zipfile
import buildconfig


def main():
    if not buildconfig.substs.get('MOZ_CODE_COVERAGE') or not buildconfig.substs.get('MOZ_RUST_TESTS'):
        return

    assert 'GRCOV_PATH' in os.environ, 'The environment variable GRCOV_PATH should contain a path to grcov'
    grcov_path = os.environ['GRCOV_PATH']
    assert os.path.exists(grcov_path), 'grcov should exist'

    grcov_command = [
        grcov_path,
        '-t', 'lcov',
        '-p', buildconfig.topsrcdir,
        buildconfig.topobjdir
    ]

    # We want to ignore system headers in our reports.
    if buildconfig.substs['OS_TARGET'] == 'WINNT':
        # We use WINDOWSSDKDIR to find the directory holding the system headers on Windows.
        windows_sdk_dir = None
        config_opts = buildconfig.substs['MOZ_CONFIGURE_OPTIONS'].split(' ')
        for opt in config_opts:
            if opt.startswith('WINDOWSSDKDIR='):
                windows_sdk_dir = opt[len('WINDOWSSDKDIR='):]
                break

        assert windows_sdk_dir is not None, 'WINDOWSSDKDIR should be in MOZ_CONFIGURE_OPTIONS'

        ignore_dir_abs = os.path.dirname(windows_sdk_dir)

        # globs passed to grcov must exist and must be relative to the source directory.
        # If it doesn't exist, maybe it has moved and we need to update the paths above.
        # If it is no longer relative to the source directory, we no longer need to ignore it and
        # this code can be removed.
        assert os.path.isdir(ignore_dir_abs), '{} is not a directory'.format(ignore_dir_abs)
        assert ignore_dir_abs.startswith(buildconfig.topsrcdir), '{} should start with {}'.format(ignore_dir_abs, buildconfig.topsrcdir)

        grcov_command += ['--ignore-dir', os.path.relpath(ignore_dir_abs, buildconfig.topsrcdir) + '*']

    if buildconfig.substs['OS_TARGET'] == 'Linux':
        gcc_dir = os.path.join(os.environ['MOZ_FETCHES_DIR'], 'gcc')
        if 'LD_LIBRARY_PATH' in os.environ:
            os.environ['LD_LIBRARY_PATH'] = '{}/lib64/:{}'.format(gcc_dir, os.environ['LD_LIBRARY_PATH'])
        else:
            os.environ['LD_LIBRARY_PATH'] = '{}/lib64/'.format(gcc_dir)

        os.environ['PATH'] = '{}/bin/:{}'.format(gcc_dir, os.environ['PATH'])

    grcov_output = subprocess.check_output(grcov_command)

    grcov_zip_path = os.path.join(buildconfig.topobjdir, 'code-coverage-grcov.zip')
    with zipfile.ZipFile(grcov_zip_path, 'a', zipfile.ZIP_DEFLATED) as z:
        z.writestr('grcov_lcov_output.info', grcov_output)


if __name__ == '__main__':
    main()
