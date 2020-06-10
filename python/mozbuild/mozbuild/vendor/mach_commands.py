# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase

@CommandProvider
class Vendor(MachCommandBase):
    """Vendor third-party dependencies into the source repository."""

    @Command('vendor', category='misc',
             description='Vendor third-party dependencies into the source repository.')
    def vendor(self):
        self._sub_mach(['help', 'vendor'])
        return 1

    @SubCommand('vendor', 'rust',
                description='Vendor rust crates from crates.io into third_party/rust')
    @CommandArgument('--ignore-modified', action='store_true',
                     help='Ignore modified files in current checkout',
                     default=False)
    @CommandArgument(
        '--build-peers-said-large-imports-were-ok', action='store_true',
        help=('Permit overly-large files to be added to the repository. '
              'To get permission to set this, raise a question in the #build '
              'channel at https://chat.mozilla.org.'),
        default=False)
    def vendor_rust(self, **kwargs):
        from mozbuild.vendor_rust import VendorRust
        vendor_command = self._spawn(VendorRust)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'aom',
                description='Vendor av1 video codec reference implementation into the '
                'source repository.')
    @CommandArgument('-r', '--revision',
                     help='Repository tag or commit to update to.')
    @CommandArgument('--repo',
                     help='Repository url to pull a snapshot from. '
                     'Supports github and googlesource.')
    @CommandArgument('--ignore-modified', action='store_true',
                     help='Ignore modified files in current checkout',
                     default=False)
    def vendor_aom(self, **kwargs):
        from mozbuild.vendor_aom import VendorAOM
        vendor_command = self._spawn(VendorAOM)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'dav1d',
                description='Vendor dav1d implementation of AV1 into the source repository.')
    @CommandArgument('-r', '--revision',
                     help='Repository tag or commit to update to.')
    @CommandArgument('--repo',
                     help='Repository url to pull a snapshot from. Supports gitlab.')
    @CommandArgument('--ignore-modified', action='store_true',
                     help='Ignore modified files in current checkout',
                     default=False)
    def vendor_dav1d(self, **kwargs):
        from mozbuild.vendor_dav1d import VendorDav1d
        vendor_command = self._spawn(VendorDav1d)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'python',
                description='Vendor Python packages from pypi.org into third_party/python')
    @CommandArgument('--with-windows-wheel', action='store_true',
                     help='Vendor a wheel for Windows along with the source package',
                     default=False)
    @CommandArgument('packages', default=None, nargs='*',
                     help='Packages to vendor. If omitted, packages and their dependencies '
                     'defined in Pipfile.lock will be vendored. If Pipfile has been modified, '
                     'then Pipfile.lock will be regenerated. Note that transient dependencies '
                     'may be updated when running this command.')
    def vendor_python(self, **kwargs):
        from mozbuild.vendor_python import VendorPython
        vendor_command = self._spawn(VendorPython)
        vendor_command.vendor(**kwargs)

    @SubCommand('vendor', 'manifest',
                description='Vendor externally hosted repositories into this '
                            'repository.')
    @CommandArgument('files', nargs='+',
                     help='Manifest files to work on')
    @CommandArgumentGroup('verify')
    @CommandArgument('--verify', '-v', action='store_true', group='verify',
                     required=True, help='Verify manifest')
    def vendor_manifest(self, files, verify):
        from mozbuild.vendor_manifest import verify_manifests
        verify_manifests(files)