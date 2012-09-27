# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import operator
import os

from mach.base import ArgumentProvider
from mozbuild.base import MozbuildObject


class Warnings(MozbuildObject, ArgumentProvider):
    """Provide commands for inspecting warnings."""

    @property
    def database_path(self):
        return self._get_state_filename('warnings.json')

    @property
    def database(self):
        from mozbuild.compilation.warnings import WarningsDatabase

        path = self.database_path

        database = WarningsDatabase()

        if os.path.exists(path):
            database.load_from_file(path)

        return database

    def summary(self, report=None):
        database = self.database

        type_counts = database.type_counts
        sorted_counts = sorted(type_counts.iteritems(),
            key=operator.itemgetter(1))

        total = 0
        for k, v in sorted_counts:
            print('%d\t%s' % (v, k))
            total += v

        print('%d\tTotal' % total)

    def list(self, report=None):
        database = self.database

        by_name = sorted(database.warnings)

        for warning in by_name:
            filename = warning['filename']

            if filename.startswith(self.topsrcdir):
                filename = filename[len(self.topsrcdir) + 1:]

            if warning['column'] is not None:
                print('%s:%d:%d [%s] %s' % (filename, warning['line'],
                    warning['column'], warning['flag'], warning['message']))
            else:
                print('%s:%d [%s] %s' % (filename, warning['line'],
                    warning['flag'], warning['message']))

    @staticmethod
    def populate_argparse(parser):
        summary = parser.add_parser('warnings-summary',
            help='Show a summary of compiler warnings.')

        summary.add_argument('report', default=None, nargs='?',
            help='Warnings report to display. If not defined, show '
                 'the most recent report')

        summary.set_defaults(cls=Warnings, method='summary', report=None)

        lst = parser.add_parser('warnings-list',
            help='Show a list of compiler warnings')
        lst.add_argument('report', default=None, nargs='?',
            help='Warnings report to display. If not defined, show '
                 'the most recent report.')

        lst.set_defaults(cls=Warnings, method='list', report=None)
