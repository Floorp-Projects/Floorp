# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

import mozfile
from mach.decorators import Command, CommandArgument
from mach.site import MozSiteMetadata


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
def install_moz_phab(command_context, force=False):
    import logging
    import os
    import re
    import subprocess
    import sys

    existing = mozfile.which("moz-phab")
    if existing and not force:
        command_context.log(
            logging.ERROR,
            "already_installed",
            {},
            "moz-phab is already installed in %s." % existing,
        )
        sys.exit(1)

    active_metadata = MozSiteMetadata.from_runtime()
    original_python = active_metadata.original_python.python_path
    is_external_python_virtualenv = (
        subprocess.check_output(
            [
                original_python,
                "-c",
                "import sys; print(sys.prefix != sys.base_prefix)",
            ]
        ).strip()
        == b"True"
    )

    # pip3 is part of Python since 3.4, however some distros choose to
    # remove core components from languages, so show a useful error message
    # if pip3 is missing.
    has_pip = subprocess.run([original_python, "-c", "import pip"]).returncode == 0
    if not has_pip:
        command_context.log(
            logging.ERROR,
            "pip_not_installed",
            {},
            "Python 3's `pip` is not installed. Try installing it with your system "
            "package manager.",
        )
        sys.exit(1)

    command = [original_python, "-m", "pip", "install", "--upgrade", "MozPhab"]

    if (
        sys.platform.startswith("linux")
        or sys.platform.startswith("openbsd")
        or sys.platform.startswith("dragonfly")
        or sys.platform.startswith("freebsd")
        or sys.platform.startswith("netbsd")
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
        command_context.log(
            logging.WARNING,
            "unsupported_platform",
            {},
            "Unsupported platform (%s), assuming per-user installation is "
            "preferred." % sys.platform,
        )
        platform_prefers_user_install = True

    command_env = os.environ.copy()

    if platform_prefers_user_install and not is_external_python_virtualenv:
        # Virtual environments don't see user packages, so only perform a user
        # installation if we're not within one.
        command.append("--user")
        # This is needed to work around a problem on Ubuntu 23.04 and Debian 12
        # See bug 1831442 for more details
        command_env["PIP_BREAK_SYSTEM_PACKAGES"] = "1"

    command_context.log(logging.INFO, "run", {}, "Installing moz-phab")
    subprocess.run(command, env=command_env)

    # There isn't an elegant way of determining the CLI location of a pip-installed package.
    # The viable mechanism used here is to:
    # 1. Get the list of info about the installed package via pip
    # 2. Parse out the install location. This gives us the python environment in which the
    #    package is installed
    # 3. Parse out the relative location of the cli script
    # 4. Join the two paths, and execute the script at that location

    info = subprocess.check_output(
        [original_python, "-m", "pip", "show", "-f", "MozPhab"],
        universal_newlines=True,
    )
    mozphab_package_location = re.compile(r"Location: (.*)").search(info).group(1)
    # This needs to match "moz-phab" (*nix) and "moz-phab.exe" (Windows) while missing
    # "moz-phab-script.py" (Windows).
    potential_cli_paths = re.compile(
        r"([^\s]*moz-phab(?:\.exe)?)$", re.MULTILINE
    ).findall(info)

    if len(potential_cli_paths) != 1:
        command_context.log(
            logging.WARNING,
            "no_mozphab_console_script",
            {},
            "Could not find the CLI script for moz-phab. Skipping install-certificate step.",
        )
        sys.exit(1)

    console_script = os.path.realpath(
        os.path.join(mozphab_package_location, potential_cli_paths[0])
    )
    subprocess.run([console_script, "install-certificate"])
