# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

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
