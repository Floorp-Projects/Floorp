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
import re
import subprocess
import sys

class VendorRust(MozbuildObject):
    def get_cargo_path(self):
        try:
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

    def check_cargo_vendor_version(self, cargo):
        '''
        Ensure that cargo-vendor is new enough. cargo-vendor 0.1.3 and newer
        strips out .gitattributes files which we want.
        '''
        for l in subprocess.check_output([cargo, 'install', '--list']).splitlines():
            # The line looks like `cargo-vendor v0.1.3:`
            m = re.match('cargo-vendor v((\d\.)*\d):', l)
            if m:
                version = m.group(1)
                return LooseVersion(version) >= b'0.1.3'
        return False

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

    def check_openssl(self):
        '''
        Set environment flags for building with openssl.

        MacOS doesn't include openssl, but the openssl-sys crate used by
        mach-vendor expects one of the system. It's common to have one
        installed in /usr/local/opt/openssl by homebrew, but custom link
        flags are necessary to build against it.
        '''

        test_paths = ['/usr/include', '/usr/local/include']
        if any([os.path.exists(os.path.join(path, 'openssl/ssl.h')) for path in test_paths]):
            # Assume we can use one of these system headers.
            return None

        if os.path.exists('/usr/local/opt/openssl/include/openssl/ssl.h'):
            # Found a likely homebrew install.
            self.log(logging.INFO, 'openssl', {},
                    'Using OpenSSL in /usr/local/opt/openssl')
            return {
                 'OPENSSL_INCLUDE_DIR': '/usr/local/opt/openssl/include',
                 'DEP_OPENSSL_INCLUDE': '/usr/local/opt/openssl/include',
            }

        self.log(logging.ERROR, 'openssl', {}, "OpenSSL not found!")
        return None

    def vendor(self, ignore_modified=False,
               build_peers_said_large_imports_were_ok=False):
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
            self.log(logging.INFO, 'installing', {}, 'Installing cargo-vendor (this may take a few minutes)...')
            env = self.check_openssl()
            self.run_process(args=[cargo, 'install', 'cargo-vendor'],
                             append_env=env)
        elif not self.check_cargo_vendor_version(cargo):
            self.log(logging.INFO, 'cargo_vendor', {}, 'cargo-vendor >= 0.1.3 required; force-reinstalling (this may take a few minutes)...')
            env = self.check_openssl()
            self.run_process(args=[cargo, 'install', '--force', 'cargo-vendor'],
                             append_env=env)
        else:
            self.log(logging.DEBUG, 'cargo_vendor', {}, 'sufficiently new cargo-vendor is already intalled')
        relative_vendor_dir = 'third_party/rust'
        vendor_dir = mozpath.join(self.topsrcdir, relative_vendor_dir)
        self.log(logging.INFO, 'rm_vendor_dir', {}, 'rm -rf %s' % vendor_dir)
        mozfile.remove(vendor_dir)
        # Once we require a new enough cargo to switch to workspaces, we can
        # just do this once on the workspace root crate.
        crates_and_roots = (
            ('gkrust', 'toolkit/library/rust'),
            ('gkrust-gtest', 'toolkit/library/gtest/rust'),
            ('mozjs_sys', 'js/src'),
        )
        for (lib, crate_root) in crates_and_roots:
            path = mozpath.join(self.topsrcdir, crate_root)
            # We do an |update -p| here to regenerate the Cargo.lock file with minimal changes. See bug 1324462
            self._run_command_in_srcdir(args=[cargo, 'update', '--manifest-path', mozpath.join(path, 'Cargo.toml'), '-p', lib])
            self._run_command_in_srcdir(args=[cargo, 'vendor', '--sync', mozpath.join(path, 'Cargo.lock'), vendor_dir])
        self.repository.add_remove_files(vendor_dir)

        # 100k is a reasonable upper bound on source file size.
        FILESIZE_LIMIT = 100 * 1024
        large_files = set()
        cumulative_added_size = 0
        for f in self.repository.get_added_files():
            path = mozpath.join(self.topsrcdir, f)
            size = os.stat(path).st_size
            cumulative_added_size += size
            if size > FILESIZE_LIMIT:
                large_files.add(f)

        # Forcefully complain about large files being added, as history has
        # shown that large-ish files typically are not needed.
        if large_files and not build_peers_said_large_imports_were_ok:
            self.log(logging.ERROR, 'filesize_check', {},
                     '''The following files exceed the filesize limit of {size}:

{files}

Please find a way to reduce the sizes of these files or talk to a build
peer about the particular large files you are adding.

The changes from `mach vendor rust` will NOT be added to version control.
'''.format(files='\n'.join(sorted(large_files)), size=FILESIZE_LIMIT))
            self.repository.forget_add_remove_files()
            sys.exit(1)

        # Only warn for large imports, since we may just have large code
        # drops from time to time (e.g. importing features into m-c).
        SIZE_WARN_THRESHOLD = 5 * 1024 * 1024
        if cumulative_added_size >= SIZE_WARN_THRESHOLD:
            self.log(logging.WARN, 'filesize_check', {},
                     '''Your changes add {size} bytes of added files.

Please consider finding ways to reduce the size of the vendored packages.
For instance, check the vendored packages for unusually large test or
benchmark files that don't need to be published to crates.io and submit
a pull request upstream to ignore those files when publishing.'''.format(size=cumulative_added_size))
