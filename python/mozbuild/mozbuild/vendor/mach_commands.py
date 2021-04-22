# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys
import logging

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase
from mozbuild.vendor.moz_yaml import load_moz_yaml, MozYamlVerifyError


@CommandProvider
class Vendor(MachCommandBase):
    """Vendor third-party dependencies into the source repository."""

    # Fun quirk of ./mach - you can specify a default argument as well as subcommands.
    # If the default argument matches a subcommand, the subcommand gets called. If it
    # doesn't, we wind up in the default command.
    @Command(
        "vendor",
        category="misc",
        description="Vendor third-party dependencies into the source repository.",
    )
    @CommandArgument(
        "--check-for-update",
        action="store_true",
        help="For scripted use, prints the new commit to update to, or nothing if up to date.",
        default=False,
    )
    @CommandArgument(
        "--ignore-modified",
        action="store_true",
        help="Ignore modified files in current checkout",
        default=False,
    )
    @CommandArgument("-r", "--revision", help="Repository tag or commit to update to.")
    @CommandArgument(
        "--verify", "-v", action="store_true", help="(Only) verify the manifest"
    )
    @CommandArgument(
        "library", nargs=1, help="The moz.yaml file of the library to vendor."
    )
    def vendor(
        self,
        command_context,
        library,
        revision,
        ignore_modified=False,
        check_for_update=False,
        verify=False,
    ):
        """
        Vendor third-party dependencies into the source repository.

        Vendoring rust and python can be done with ./mach vendor [rust/python].
        Vendoring other libraries can be done with ./mach vendor [arguments] path/to/file.yaml
        """
        library = library[0]
        assert library not in ["rust", "python"]

        self.populate_logger()
        self.log_manager.enable_unstructured()
        if check_for_update:
            logging.disable()

        try:
            manifest = load_moz_yaml(library)
            if verify:
                print("%s: OK" % library)
                sys.exit(0)
        except MozYamlVerifyError as e:
            print(e)
            sys.exit(1)

        if not ignore_modified and not check_for_update:
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

    # =====================================================================

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
    def vendor_rust(self, command_context, **kwargs):
        from mozbuild.vendor.vendor_rust import VendorRust

        vendor_command = self._spawn(VendorRust)
        vendor_command.vendor(**kwargs)

    # =====================================================================

    @SubCommand(
        "vendor",
        "python",
        description="Vendor Python packages from pypi.org into third_party/python. "
        "Some extra files like docs and tests will automatically be excluded.",
    )
    @CommandArgument(
        "--keep-extra-files",
        action="store_true",
        default=False,
        help="Keep all files, including tests and documentation.",
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
    def vendor_python(self, command_context, **kwargs):
        from mozbuild.vendor.vendor_python import VendorPython

        vendor_command = self._spawn(VendorPython)
        vendor_command.vendor(**kwargs)
