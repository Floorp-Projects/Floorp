# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from distutils.version import LooseVersion
import logging
from mozbuild.base import (
    BuildEnvironmentNotFoundException,
    MozbuildObject,
)
import mozfile
import mozpack.path as mozpath
import os
import subprocess
import sys

class VendorRust(MozbuildObject):
    def get_cargo_path(self):
        try:
            # If the build isn't --enable-rust then CARGO won't be set.
            return self.substs['CARGO']
        except (BuildEnvironmentNotFoundException, KeyError):
            # Default if this tree isn't configured.
            import which
            return which.which('cargo')

    def check_cargo_version(self, cargo):
        '''
        Ensure that cargo is new enough. cargo 0.12 added support
        for source replacement, which is required for vendoring to work.
        '''
        out = subprocess.check_output([cargo, '--version']).splitlines()[0]
        if not out.startswith('cargo'):
            return False
        return LooseVersion(out.split()[1]) >= b'0.13'

    def check_modified_files(self):
        '''
        Ensure that there aren't any uncommitted changes to files
        in the working copy, since we're going to change some state
        on the user. Allow changes to Cargo.{toml,lock} since that's
        likely to be a common use case.
        '''
        modified = [f for f in self.repository.get_modified_files() if os.path.basename(f) not in ('Cargo.toml', 'Cargo.lock')]
        if modified:
            self.log(logging.ERROR, 'modified_files', {},
                     '''You have uncommitted changes to the following files:

{files}

Please commit or stash these changes before vendoring, or re-run with `--ignore-modified`.
'''.format(files='\n'.join(sorted(modified))))
            sys.exit(1)

    def vendor(self, ignore_modified=False):
        self.populate_logger()
        self.log_manager.enable_unstructured()
        if not ignore_modified:
            self.check_modified_files()
        cargo = self.get_cargo_path()
        if not self.check_cargo_version(cargo):
            self.log(logging.ERROR, 'cargo_version', {}, 'Cargo >= 0.13 required (install Rust 1.12 or newer)')
            return
        else:
            self.log(logging.DEBUG, 'cargo_version', {}, 'cargo is new enough')
        have_vendor = any(l.strip() == 'vendor' for l in subprocess.check_output([cargo, '--list']).splitlines())
        if not have_vendor:
            self.log(logging.INFO, 'installing', {}, 'Installing cargo-vendor')
            self.run_process(args=[cargo, 'install', 'cargo-vendor'])
        else:
            self.log(logging.DEBUG, 'cargo_vendor', {}, 'cargo-vendor already intalled')
        vendor_dir = mozpath.join(self.topsrcdir, 'third_party/rust')
        self.log(logging.INFO, 'rm_vendor_dir', {}, 'rm -rf %s' % vendor_dir)
        mozfile.remove(vendor_dir)
        # Once we require a new enough cargo to switch to workspaces, we can
        # just do this once on the workspace root crate.
        for crate_root in ('toolkit/library/rust/',
                           'toolkit/library/gtest/rust'):
            path = mozpath.join(self.topsrcdir, crate_root)
            self._run_command_in_srcdir(args=[cargo, 'generate-lockfile', '--manifest-path', mozpath.join(path, 'Cargo.toml')])
            self._run_command_in_srcdir(args=[cargo, 'vendor', '--sync', mozpath.join(path, 'Cargo.lock'), vendor_dir])
        #TODO: print stats on size of files added/removed, warn or error
        # when adding very large files (bug 1306078)
        self.repository.add_remove_files(vendor_dir)
