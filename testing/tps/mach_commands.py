# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function
import os

from mach.decorators import Command, CommandArgument, CommandProvider
from mozbuild.base import MachCommandBase
from mozpack.copier import Jarrer
from mozpack.files import FileFinder


@CommandProvider
class MachCommands(MachCommandBase):
    """TPS tests for Sync."""

    @Command('tps-build', category='testing', description='Build TPS add-on.')
    @CommandArgument('--dest', default=None, help='Where to write add-on.')
    def build(self, dest):
        src = os.path.join(self.topsrcdir, 'services', 'sync', 'tps', 'extensions', 'tps')
        dest = os.path.join(dest or os.path.join(self.topobjdir, 'services', 'sync'), 'tps.xpi')

        if not os.path.exists(os.path.dirname(dest)):
            os.makedirs(os.path.dirname(dest))

        jarrer = Jarrer(optimize=False)
        for p, f in FileFinder(src).find('*'):
            jarrer.add(p, f)
        jarrer.copy(dest)

        print('Built TPS add-on as %s' % dest)
