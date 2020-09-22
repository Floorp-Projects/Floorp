# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

import mozfile
from mach.decorators import CommandProvider, Command, CommandArgument
from mozbuild.base import MachCommandBase


@CommandProvider
class PhabricatorCommandProvider(MachCommandBase):
    @Command(
        "install-moz-phab",
        category="misc",
        description="Install patch submission tool.",
    )
    @CommandArgument(
        "--force",
        "-f",
        action="store_true",
        help="Force installation even if already installed.",
    )
    def install_moz_phab(self, force=False):
        import logging
        import os
        import subprocess
        import sys
        import pkg_resources

        existing = mozfile.which("moz-phab")
        if existing and not force:
            self.log(
                logging.ERROR,
                "already_installed",
                {},
                "moz-phab is already installed in %s." % existing,
            )
            sys.exit(1)

        # pip3 is part of Python since 3.4, however some distros choose to
        # remove core components from languages, so show a useful error message
        # if pip3 is missing.
        pip3 = mozfile.which("pip3")
        if not pip3:
            self.log(
                logging.ERROR,
                "pip3_not_installed",
                {},
                "`pip3` is not installed. Try installing it with your system "
                "package manager.",
            )
            sys.exit(1)

        command = [pip3, "install", "--upgrade", "MozPhab"]

        if (
            sys.platform.startswith("linux")
            or sys.platform.startswith("openbsd")
            or sys.platform.startswith("dragonfly")
            or sys.platform.startswith("freebsd")
        ):
            # On all Linux and BSD distros we consider doing a user installation.
            platform_prefers_user_install = True

        elif sys.platform.startswith("darwin"):
            # On MacOS we require brew or ports, which work better without --user.
            platform_prefers_user_install = False

        elif sys.platform.startswith("win32") or sys.platform.startswith("msys"):
            # Likewise for Windows we assume a system level install is preferred.
            platform_prefers_user_install = False

        else:
            # Unsupported, default to --user.
            self.log(
                logging.WARNING,
                "unsupported_platform",
                {},
                "Unsupported platform (%s), assuming per-user installation is "
                "preferred."
                % sys.platform,
            )
            platform_prefers_user_install = True

        if platform_prefers_user_install and not os.environ.get('VIRTUAL_ENV'):
            # Virtual environments don't see user packages, so only perform a user
            # installation if we're not within one.
            command.append("--user")

        self.log(logging.INFO, "run", {}, "Installing moz-phab")
        subprocess.run(command)

        dist = pkg_resources.get_distribution('mozphab')

        # "get_metadata_lines('RECORD')" shows us all the files (paths and hashes) used by this
        # package. Fetch them and strip off the hash.
        package_files = [file.split(',')[0] for file in dist.get_metadata_lines('RECORD')]
        potential_cli_paths = [file for file in package_files
                               if os.path.basename(file) in ('moz-phab.exe', 'moz-phab')]

        if len(potential_cli_paths) != 1:
            self.log(
                logging.WARNING,
                "no_mozphab_console_script",
                {},
                "Could not find the CLI script for moz-phab. Skipping install-certificate step."
            )
            sys.exit(1)

        console_script = os.path.realpath(os.path.join(dist.location, potential_cli_paths[0]))
        subprocess.run([console_script, 'install-certificate'])
