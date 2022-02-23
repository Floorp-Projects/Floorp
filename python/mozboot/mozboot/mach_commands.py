# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import errno
import sys

from pathlib import Path
from mach.decorators import CommandArgument, Command
from mozboot.bootstrap import APPLICATIONS


@Command(
    "bootstrap",
    category="devenv",
    description="Install required system packages for building.",
)
@CommandArgument(
    "--application-choice",
    choices=list(APPLICATIONS.keys()) + list(APPLICATIONS.values()),
    default=None,
    help="Pass in an application choice instead of using the default "
    "interactive prompt.",
)
@CommandArgument(
    "--no-system-changes",
    dest="no_system_changes",
    action="store_true",
    help="Only execute actions that leave the system configuration alone.",
)
def bootstrap(command_context, application_choice=None, no_system_changes=False):
    """Bootstrap system and mach for optimal development experience."""
    from mozboot.bootstrap import Bootstrapper

    bootstrapper = Bootstrapper(
        choice=application_choice,
        no_interactive=not command_context._mach_context.is_interactive,
        no_system_changes=no_system_changes,
        mach_context=command_context._mach_context,
    )
    bootstrapper.bootstrap(command_context.settings)


@Command(
    "vcs-setup",
    category="devenv",
    description="Help configure a VCS for optimal development.",
)
@CommandArgument(
    "-u",
    "--update-only",
    action="store_true",
    help="Only update recommended extensions, don't run the wizard.",
)
def vcs_setup(command_context, update_only=False):
    """Ensure a Version Control System (Mercurial or Git) is optimally
    configured.

    This command will inspect your VCS configuration and
    guide you through an interactive wizard helping you configure the
    VCS for optimal use on Mozilla projects.

    User choice is respected: no changes are made without explicit
    confirmation from you.

    If "--update-only" is used, the interactive wizard is disabled
    and this command only ensures that remote repositories providing
    VCS extensions are up to date.
    """
    import mozboot.bootstrap as bootstrap
    import mozversioncontrol
    from mozfile import which
    from mach.util import to_optional_path

    repo = mozversioncontrol.get_repository_object(command_context._mach_context.topdir)
    tool = "hg"
    if repo.name == "git":
        tool = "git"

    # "hg" is an executable script with a shebang, which will be found by
    # which. We need to pass a win32 executable to the function because we
    # spawn a process from it.
    if sys.platform in ("win32", "msys"):
        tool += ".exe"

    vcs = to_optional_path(which(tool))
    if not vcs:
        raise OSError(errno.ENOENT, "Could not find {} on $PATH".format(tool))

    if update_only:
        if repo.name == "git":
            bootstrap.update_git_tools(
                vcs,
                Path(command_context._mach_context.state_dir),
            )
        else:
            bootstrap.update_vct(vcs, Path(command_context._mach_context.state_dir))
    else:
        if repo.name == "git":
            bootstrap.configure_git(
                vcs,
                to_optional_path(which("git-cinnabar")),
                Path(command_context._mach_context.state_dir),
                Path(command_context._mach_context.topdir),
            )
        else:
            bootstrap.configure_mercurial(
                vcs, Path(command_context._mach_context.state_dir)
            )
