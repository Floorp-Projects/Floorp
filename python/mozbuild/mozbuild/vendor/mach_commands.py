# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys
import logging

from mach.decorators import CommandArgument, CommandProvider, Command, SubCommand

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
        "--add-to-exports",
        action="store_true",
        help="Will attempt to add new header files into any relevant EXPORTS block",
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
        add_to_exports=False,
        verify=False,
    ):
        """
        Vendor third-party dependencies into the source repository.

        Vendoring rust and python can be done with ./mach vendor [rust/python].
        Vendoring other libraries can be done with ./mach vendor [arguments] path/to/file.yaml
        """
        library = library[0]
        assert library not in ["rust", "python"]

        command_context.populate_logger()
        command_context.log_manager.enable_unstructured()
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
            self.check_modified_files(command_context)
        if not revision:
            revision = "HEAD"

        from mozbuild.vendor.vendor_manifest import VendorManifest

        vendor_command = command_context._spawn(VendorManifest)
        vendor_command.vendor(
            library, manifest, revision, check_for_update, add_to_exports
        )

        sys.exit(0)

    def check_modified_files(self, command_context):
        """
        Ensure that there aren't any uncommitted changes to files
        in the working copy, since we're going to change some state
        on the user.
        """
        modified = command_context.repository.get_changed_files("M")
        if modified:
            command_context.log(
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

        vendor_command = command_context._spawn(VendorRust)
        vendor_command.vendor(**kwargs)

    # =====================================================================

    @SubCommand(
        "vendor",
        "python",
        description="Vendor Python packages from pypi.org into third_party/python. "
        "Some extra files like docs and tests will automatically be excluded."
        "Installs the packages listed in third_party/python/requirements.in and "
        "their dependencies.",
    )
    @CommandArgument(
        "--keep-extra-files",
        action="store_true",
        default=False,
        help="Keep all files, including tests and documentation.",
    )
    def vendor_python(self, command_context, **kwargs):
        from mozbuild.vendor.vendor_python import VendorPython

        if sys.version_info[:2] != (3, 6):
            print(
                "You must use Python 3.6 to vendor Python packages. If you don't "
                "have Python 3.6, you can request that your package be added by "
                "creating a bug: \n"
                "https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox%20Build%20System&component=Mach%20Core"  # noqa F401
            )
            return 1

        vendor_command = command_context._spawn(VendorPython)
        vendor_command.vendor(**kwargs)
