# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
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
        config_paths = ['~/.hgrc']
        if sys.platform in ('win32', 'cygwin'):
            config_paths.insert(0, '~/mercurial.ini')
        config_paths = map(os.path.expanduser, config_paths)

        if update_only:
            from hgsetup.update import MercurialUpdater
            updater = MercurialUpdater(self._context.state_dir)
            result = updater.update_all()
        else:
            from hgsetup.wizard import MercurialSetupWizard
            wizard = MercurialSetupWizard(self._context.state_dir)
            result = wizard.run(map(os.path.expanduser, config_paths))

        if result:
            print('(despite the failure, mach will not nag you to run '
                  '`mach mercurial-setup`)')

        return result
