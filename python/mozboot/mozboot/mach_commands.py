# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)


@CommandProvider
class Bootstrap(object):
    """Bootstrap system and mach for optimal development experience."""
    def __init__(self, context):
        self._context = context

    @Command('bootstrap', category='devenv',
             description='Install required system packages for building.')
    @CommandArgument('--application-choice',
                     default=None,
                     help='Pass in an application choice (see mozboot.bootstrap.APPLICATIONS) '
                     'instead of using the default interactive prompt.')
    @CommandArgument('--no-interactive', dest='no_interactive', action='store_true',
                     help='Answer yes to any (Y/n) interactive prompts.')
    @CommandArgument('--no-system-changes', dest='no_system_changes',
                     action='store_true',
                     help='Only execute actions that leave the system '
                          'configuration alone.')
    def bootstrap(self, application_choice=None, no_interactive=False, no_system_changes=False):
        from mozboot.bootstrap import Bootstrapper

        bootstrapper = Bootstrapper(
            choice=application_choice,
            no_interactive=no_interactive,
            no_system_changes=no_system_changes,
            mach_context=self._context,
        )
        bootstrapper.bootstrap()


@CommandProvider
class VersionControlCommands(object):
    def __init__(self, context):
        self._context = context

    @Command('vcs-setup', category='devenv',
             description='Help configure a VCS for optimal development.')
    @CommandArgument('-u', '--update-only', action='store_true',
                     help='Only update recommended extensions, don\'t run the wizard.')
    def vcs_setup(self, update_only=False):
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
        import which
        import mozboot.bootstrap as bootstrap
        import mozversioncontrol

        repo = mozversioncontrol.get_repository_object(self._context.topdir)
        vcs = 'hg'
        if repo.name == 'git':
            vcs = 'git'

        # "hg" is an executable script with a shebang, which will be found
        # by which.which. We need to pass a win32 executable to the function
        # because we spawn a process
        # from it.
        if sys.platform in ('win32', 'msys'):
            vcs = which.which(vcs + '.exe')
        else:
            vcs = which.which(vcs)

        if update_only:
            if repo.name == 'git':
                bootstrap.update_git_tools(vcs, self._context.state_dir, self._context.topdir)
            else:
                bootstrap.update_vct(vcs, self._context.state_dir)
        else:
            if repo.name == 'git':
                bootstrap.configure_git(vcs, self._context.state_dir, self._context.topdir)
            else:
                bootstrap.configure_mercurial(vcs, self._context.state_dir)
