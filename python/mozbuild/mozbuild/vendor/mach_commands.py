# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys
import logging

from mach.decorators import (
    CommandArgument,
    CommandArgumentGroup,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase
from mozbuild.vendor.moz_yaml import load_moz_yaml, MozYamlVerifyError


@CommandProvider
class Vendor(MachCommandBase):
    """Vendor third-party dependencies into the source repository."""

    @Command(
        "vendor",
        category="misc",
        description="Vendor third-party dependencies into the source repository.",
    )
    @CommandArgument("--check-for-update", action="store_true", default=False)
    @CommandArgument(
        "--ignore-modified",
        action="store_true",
        help="Ignore modified files in current checkout",
        default=False,
    )
    @CommandArgument("-r", "--revision", help="Repository tag or commit to update to.")
    @CommandArgument("library", nargs=1)
    @CommandArgumentGroup("verify")
    @CommandArgument("--verify", "-v", action="store_true", help="Verify manifest")
    def vendor(
        self,
        library,
        revision,
        ignore_modified=False,
        check_for_update=False,
        verify=False,
    ):
        """
        Fun quirk of ./mach - you can specify a default argument as well as subcommands.
        If the default argument matches a subcommand, the subcommand gets called. If it
        doesn't, we wind up here to handle it.
        """
        library = library[0]
        assert library not in ["rust", "python"]

        self.populate_logger()
        self.log_manager.enable_unstructured()

        try:
            manifest = load_moz_yaml(library)
            if verify:
                print("%s: OK" % library)
                sys.exit(0)
        except MozYamlVerifyError as e:
            print(e)
            sys.exit(1)

        if not ignore_modified:
            self.check_modified_files()
        if not revision:
            revision = "master"

        from mozbuild.vendor.vendor_manifest import VendorManifest

        vendor_command = self._spawn(VendorManifest)
        vendor_command.vendor(library, manifest, revision, check_for_update)

        sys.exit(0)

    def check_modified_files(self):
        """
        Ensure that there aren't any uncommitted changes to files
        in the working copy, since we're going to change some state
        on the user.
        """
        modified = self.repository.get_changed_files("M")
        if modified:
            self.log(
                logging.ERROR,
                "modified_files",
                {},
                """You have uncommitted changes to the following files:

{files}

Please commit or stash these changes before vendoring, or re-run with `--ignore-modified`.
""".format(
                    files="\n".join(sorted(modified))
                ),
            )
            sys.exit(1)

    @SubCommand(
        "vendor",
        "rust",
        description="Vendor rust crates from crates.io into third_party/rust",
    )
    @CommandArgument(
        "--ignore-modified",
        action="store_true",
        help="Ignore modified files in current checkout",
        default=False,
    )
    @CommandArgument(
        "--build-peers-said-large-imports-were-ok",
        action="store_true",
        help=(
            "Permit overly-large files to be added to the repository. "
            "To get permission to set this, raise a question in the #build "
            "channel at https://chat.mozilla.org."
        ),
        default=False,
    )
    def vendor_rust(self, **kwargs):
        from mozbuild.vendor.vendor_rust import VendorRust

        vendor_command = self._spawn(VendorRust)
        vendor_command.vendor(**kwargs)

    @SubCommand(
        "vendor",
        "aom",
        description="Vendor av1 video codec reference implementation into the "
        "source repository.",
    )
    @CommandArgument("-r", "--revision", help="Repository tag or commit to update to.")
    @CommandArgument(
        "--repo",
        help="Repository url to pull a snapshot from. "
        "Supports github and googlesource.",
    )
    @CommandArgument(
        "--ignore-modified",
        action="store_true",
        help="Ignore modified files in current checkout",
        default=False,
    )
    def vendor_aom(self, **kwargs):
        from mozbuild.vendor.vendor_aom import VendorAOM

        vendor_command = self._spawn(VendorAOM)
        vendor_command.vendor(**kwargs)

    @SubCommand(
        "vendor",
        "dav1d",
        description="Vendor dav1d implementation of AV1 into the source repository.",
    )
    @CommandArgument("-r", "--revision", help="Repository tag or commit to update to.")
    @CommandArgument(
        "--repo", help="Repository url to pull a snapshot from. Supports gitlab."
    )
    @CommandArgument(
        "--ignore-modified",
        action="store_true",
        help="Ignore modified files in current checkout",
        default=False,
    )
    def vendor_dav1d(self, **kwargs):
        from mozbuild.vendor.vendor_dav1d import VendorDav1d

        vendor_command = self._spawn(VendorDav1d)
        vendor_command.vendor(**kwargs)

    @SubCommand(
        "vendor",
        "python",
        description="Vendor Python packages from pypi.org into third_party/python",
    )
    @CommandArgument(
        "--with-windows-wheel",
        action="store_true",
        help="Vendor a wheel for Windows along with the source package",
        default=False,
    )
    @CommandArgument(
        "packages",
        default=None,
        nargs="*",
        help="Packages to vendor. If omitted, packages and their dependencies "
        "defined in Pipfile.lock will be vendored. If Pipfile has been modified, "
        "then Pipfile.lock will be regenerated. Note that transient dependencies "
        "may be updated when running this command.",
    )
    def vendor_python(self, **kwargs):
        from mozbuild.vendor.vendor_python import VendorPython

        vendor_command = self._spawn(VendorPython)
        vendor_command.vendor(**kwargs)

    @SubCommand(
        "vendor",
        "manifest",
        description="Vendor externally hosted repositories into this " "repository.",
    )
    @CommandArgument("files", nargs="+", help="Manifest files to work on")
    @CommandArgumentGroup("verify")
    @CommandArgument(
        "--verify",
        "-v",
        action="store_true",
        group="verify",
        required=True,
        help="Verify manifest",
    )
    def vendor_manifest(self, files, verify):
        from mozbuild.vendor.vendor_manifest import verify_manifests

        verify_manifests(files)
