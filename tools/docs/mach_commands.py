# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase
from mozbuild.frontend.reader import BuildReader


@CommandProvider
class Documentation(MachCommandBase):
    """Helps manage in-tree documentation."""

    @Command('build-docs', category='build-dev',
        description='Generate documentation for the tree.')
    @CommandArgument('--format', default='html',
        help='Documentation format to write.')
    @CommandArgument('outdir', default='<DEFAULT>', nargs='?',
        help='Where to write output.')
    def build_docs(self, format=None, outdir=None):
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('sphinx_rtd_theme==0.1.6')

        import sphinx

        if outdir == '<DEFAULT>':
            outdir = os.path.join(self.topobjdir, 'docs')

        args = [
            'sphinx',
            '-b', format,
            os.path.join(self.topsrcdir, 'tools', 'docs'),
            os.path.join(outdir, format),
        ]

        return sphinx.main(args)
