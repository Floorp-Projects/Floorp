# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import logging
import os

from mach.base import ArgumentProvider
from mozbuild.base import MozbuildObject


class Build(MozbuildObject, ArgumentProvider):
    """Interface to build the tree."""

    def build(self):
        # This code is only meant to be temporary until the more robust tree
        # building code in bug 780329 lands.
        from mozbuild.compilation.warnings import WarningsCollector
        from mozbuild.compilation.warnings import WarningsDatabase

        warnings_path = self._get_state_filename('warnings.json')
        warnings_database = WarningsDatabase()

        if os.path.exists(warnings_path):
            warnings_database.load_from_file(warnings_path)

        warnings_collector = WarningsCollector(database=warnings_database,
            objdir=self.topobjdir)

        def on_line(line):
            try:
                warning = warnings_collector.process_line(line)
                if warning:
                    self.log(logging.INFO, 'compiler_warning', warning,
                        'Warning: {flag} in {filename}: {message}')
            except:
                # This will get logged in the more robust implementation.
                pass

            self.log(logging.INFO, 'build_output', {'line': line}, '{line}')

        self._run_make(srcdir=True, filename='client.mk', line_handler=on_line,
            log=False, print_directory=False)

        self.log(logging.WARNING, 'warning_summary',
            {'count': len(warnings_collector.database)},
            '{count} compiler warnings present.')

        warnings_database.save_to_file(warnings_path)

    @staticmethod
    def populate_argparse(parser):
        build = parser.add_parser('build', help='Build the tree.')
        build.set_defaults(cls=Build, method='build')
