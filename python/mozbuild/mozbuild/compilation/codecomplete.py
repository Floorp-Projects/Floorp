# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

# This modules provides functionality for dealing with code completion.

from __future__ import absolute_import

import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase
from mozbuild.shellutil import (
    split as shell_split,
    quote as shell_quote,
)


@CommandProvider
class Introspection(MachCommandBase):
    """Instropection commands."""

    @Command('compileflags', category='devenv',
        description='Display the compilation flags for a given source file')
    @CommandArgument('what', default=None,
        help='Source file to display compilation flags for')
    def compileflags(self, what):
        from mozbuild.util import resolve_target_to_make
        from mozbuild.compilation import util

        if not util.check_top_objdir(self.topobjdir):
            return 1

        path_arg = self._wrap_path_argument(what)

        make_dir, make_target = resolve_target_to_make(self.topobjdir,
            path_arg.relpath())

        if make_dir is None and make_target is None:
            return 1

        build_vars = util.get_build_vars(make_dir, self)

        if what.endswith('.c'):
            name = 'COMPILE_CFLAGS'
        else:
            name = 'COMPILE_CXXFLAGS'

        if name not in build_vars:
            return

        print(' '.join(shell_quote(arg)
                       for arg in shell_split(build_vars[name])))
