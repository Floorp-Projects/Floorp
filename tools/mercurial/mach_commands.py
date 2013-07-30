# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import sys

from mach.decorators import (
    CommandProvider,
    Command,
)


@CommandProvider
class VersionControlCommands(object):
    def __init__(self, context):
        self._context = context

    @Command('mercurial-setup', category='devenv',
        description='Help configure Mercurial for optimal development.')
    def mercurial_bootstrap(self):
        sys.path.append(os.path.dirname(__file__))

        from hgsetup.wizard import MercurialSetupWizard

        wizard = MercurialSetupWizard(self._context.state_dir)
        result = wizard.run(os.path.expanduser('~/.hgrc'))

        # Touch a file so we can periodically prompt to update extensions.
        state_path = os.path.join(self._context.state_dir,
            'mercurial/setup.lastcheck')
        with open(state_path, 'a'):
            os.utime(state_path, None)

        return result
