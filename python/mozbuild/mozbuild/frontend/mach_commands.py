# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from collections import defaultdict
import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase
import mozpack.path as mozpath


class InvalidPathException(Exception):
    """Represents an error due to an invalid path."""


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
        """Show files metadata derived from moz.build files.

        moz.build files contain "Files" sub-contexts for declaring metadata
        against file patterns. This command suite is used to query that data.
        """

    @SubCommand('file-info', 'bugzilla-component',
                'Show Bugzilla component info for files listed.')
    @CommandArgument('-r', '--rev',
                     help='Version control revision to look up info from')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_bugzilla(self, paths, rev=None):
        """Show Bugzilla component for a set of files.

        Given a requested set of files (which can be specified using
        wildcards), print the Bugzilla component for each file.
        """
        components = defaultdict(set)
        try:
            for p, m in self._get_files_info(paths, rev=rev).items():
                components[m.get('BUG_COMPONENT')].add(p)
        except InvalidPathException as e:
            print(e.message)
            return 1

        for component, files in sorted(components.items(), key=lambda x: (x is None, x)):
            print('%s :: %s' % (component.product, component.component) if component else 'UNKNOWN')
            for f in sorted(files):
                print('  %s' % f)

    @SubCommand('file-info', 'missing-bugzilla',
                'Show files missing Bugzilla component info')
    @CommandArgument('-r', '--rev',
                     help='Version control revision to look up info from')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_missing_bugzilla(self, paths, rev=None):
        try:
            for p, m in sorted(self._get_files_info(paths, rev=rev).items()):
                if 'BUG_COMPONENT' not in m:
                    print(p)
        except InvalidPathException as e:
            print(e.message)
            return 1

    @SubCommand('file-info', 'dep-tests',
                'Show test files marked as dependencies of these source files.')
    @CommandArgument('-r', '--rev',
                     help='Version control revision to look up info from')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_test_deps(self, paths, rev=None):
        try:
            for p, m in self._get_files_info(paths, rev=rev).items():
                print('%s:' % mozpath.relpath(p, self.topsrcdir))
                if m.test_files:
                    print('\tTest file patterns:')
                    for p in m.test_files:
                        print('\t\t%s' % p)
                if m.test_tags:
                    print('\tRelevant tags:')
                    for p in m.test_tags:
                        print('\t\t%s' % p)
                if m.test_flavors:
                    print('\tRelevant flavors:')
                    for p in m.test_flavors:
                        print('\t\t%s' % p)

        except InvalidPathException as e:
            print(e.message)
            return 1


    def _get_reader(self, finder):
        from mozbuild.frontend.reader import (
            BuildReader,
            EmptyConfig,
        )

        config = EmptyConfig(self.topsrcdir)
        return BuildReader(config, finder=finder)

    def _get_files_info(self, paths, rev=None):
        from mozbuild.frontend.reader import default_finder
        from mozpack.files import FileFinder, MercurialRevisionFinder

        # Normalize to relative from topsrcdir.
        relpaths = []
        for p in paths:
            a = mozpath.abspath(p)
            if not mozpath.basedir(a, [self.topsrcdir]):
                raise InvalidPathException('path is outside topsrcdir: %s' % p)

            relpaths.append(mozpath.relpath(a, self.topsrcdir))

        repo = None
        if rev:
            hg_path = os.path.join(self.topsrcdir, '.hg')
            if not os.path.exists(hg_path):
                raise InvalidPathException('a Mercurial repo is required '
                        'when specifying a revision')

            repo = self.topsrcdir

        # We need two finders because the reader's finder operates on
        # absolute paths.
        finder = FileFinder(self.topsrcdir, find_executables=False)
        if repo:
            reader_finder = MercurialRevisionFinder(repo, rev=rev,
                                                    recognize_repo_paths=True)
        else:
            reader_finder = default_finder

        # Expand wildcards.
        allpaths = []
        for p in relpaths:
            if '*' not in p:
                if p not in allpaths:
                    allpaths.append(p)
                continue

            if repo:
                raise InvalidPathException('cannot use wildcard in version control mode')

            for path, f in finder.find(p):
                if path not in allpaths:
                    allpaths.append(path)

        reader = self._get_reader(finder=reader_finder)
        return reader.files_info(allpaths)
