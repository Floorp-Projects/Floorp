# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

from collections import defaultdict

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase
import mozpack.path as mozpath


@CommandProvider
class MozbuildFileCommands(MachCommandBase):
    @Command('mozbuild-reference', category='build-dev',
        description='View reference documentation on mozbuild files.')
    @CommandArgument('symbol', default=None, nargs='*',
        help='Symbol to view help on. If not specified, all will be shown.')
    @CommandArgument('--name-only', '-n', default=False, action='store_true',
        help='Print symbol names only.')
    def reference(self, symbol, name_only=False):
        # mozbuild.sphinx imports some Sphinx modules, so we need to be sure
        # the optional Sphinx package is installed.
        self._activate_virtualenv()
        self.virtualenv_manager.install_pip_package('Sphinx==1.1.3')

        from mozbuild.sphinx import (
            format_module,
            function_reference,
            special_reference,
            variable_reference,
        )

        import mozbuild.frontend.context as m

        if name_only:
            for s in sorted(m.VARIABLES.keys()):
                print(s)

            for s in sorted(m.FUNCTIONS.keys()):
                print(s)

            for s in sorted(m.SPECIAL_VARIABLES.keys()):
                print(s)

            return 0

        if len(symbol):
            for s in symbol:
                if s in m.VARIABLES:
                    for line in variable_reference(s, *m.VARIABLES[s]):
                        print(line)
                    continue
                elif s in m.FUNCTIONS:
                    for line in function_reference(s, *m.FUNCTIONS[s]):
                        print(line)
                    continue
                elif s in m.SPECIAL_VARIABLES:
                    for line in special_reference(s, *m.SPECIAL_VARIABLES[s]):
                        print(line)
                    continue

                print('Could not find symbol: %s' % s)
                return 1

            return 0

        for line in format_module(m):
            print(line)

        return 0

    @Command('file-info', category='build-dev',
             description='Query for metadata about files.')
    def file_info(self):
        pass

    @SubCommand('file-info', 'bugzilla-component',
                'Show Bugzilla component info for files listed.')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_bugzilla(self, paths):
        components = defaultdict(set)
        for p, m in self._get_files_info(paths).items():
            components[m.get('BUG_COMPONENT')].add(p)

        for component, files in sorted(components.items(), key=lambda x: (x is None, x)):
            print('%s :: %s' % (component.product, component.component) if component else 'UNKNOWN')
            for f in sorted(files):
                print('  %s' % f)

    @SubCommand('file-info', 'missing-bugzilla',
                'Show files missing Bugzilla component info')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_missing_bugzilla(self, paths):
        for p, m in sorted(self._get_files_info(paths).items()):
            if 'BUG_COMPONENT' not in m:
                print(p)

    def _get_reader(self):
        from mozbuild.frontend.reader import BuildReader
        config = self.config_environment
        return BuildReader(config)

    def _get_files_info(self, paths):
        relpaths = []
        for p in paths:
            a = mozpath.abspath(p)
            if not mozpath.basedir(a, [self.topsrcdir]):
                print('path is not inside topsrcdir: %s' % p)
                return 1

            relpaths.append(mozpath.relpath(a, self.topsrcdir))

        reader = self._get_reader()
        return reader.files_info(relpaths)
