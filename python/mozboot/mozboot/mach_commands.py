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

    @Command('bootstrap', category='devenv',
             description='Install required system packages for building.')
    def bootstrap(self):
        from mozboot.bootstrap import Bootstrapper

        bootstrapper = Bootstrapper()
        bootstrapper.bootstrap()


@CommandProvider
class VersionControlCommands(object):
    def __init__(self, context):
        self._context = context

    @Command('mercurial-setup', category='devenv',
        description='Help configure Mercurial for optimal development.')
    @CommandArgument('-u', '--update-only', action='store_true',
        help='Only update recommended extensions, don\'t run the wizard.')
    def mercurial_setup(self, update_only=False):
        """Ensure Mercurial is optimally configured.

        This command will inspect your Mercurial configuration and
        guide you through an interactive wizard helping you configure
        Mercurial for optimal use on Mozilla projects.

        User choice is respected: no changes are made without explicit
        confirmation from you.

        If "--update-only" is used, the interactive wizard is disabled
        and this command only ensures that remote repositories providing
        Mercurial extensions are up to date.
        """
        import which
        import mozboot.bootstrap as bootstrap

        # "hg" is an executable script with a shebang, which will be found
        # be which.which. We need to pass a win32 executable to the function
        # because we spawn a process
        # from it.
        if sys.platform in ('win32', 'msys'):
            hg = which.which('hg.exe')
        else:
            hg = which.which('hg')

        if update_only:
            bootstrap.update_vct(hg, self._context.state_dir)
        else:
            bootstrap.configure_mercurial(hg, self._context.state_dir)
