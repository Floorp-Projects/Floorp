# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from collections import defaultdict
import json
import os
import sys

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
    SubCommand,
)

from mozbuild.base import MachCommandBase
import mozpack.path as mozpath

TOPSRCDIR = os.path.abspath(os.path.join(__file__, '../../../../../'))

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
    @CommandArgument('--format', choices={'json', 'plain'}, default='plain',
                     help='Output format', dest='fmt')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_bugzilla(self, paths, rev=None, fmt=None):
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

        if fmt == 'json':
            data = {}
            for component, files in components.items():
                if not component:
                    continue
                for f in files:
                    data[f] = [component.product, component.component]

            json.dump(data, sys.stdout, sort_keys=True, indent=2)
            return
        elif fmt == 'plain':
            data = sorted(components.items(),
                          key=lambda x: (x is None, x))
            for component, files in data:
                if component:
                    s = '%s :: %s' % (component.product, component.component)
                else:
                    s = 'UNKNOWN'

                print(s)
                for f in sorted(files):
                    print('  %s' % f)
        else:
            print('unhandled output format: %s' % fmt)
            return 1

    @SubCommand('file-info', 'missing-bugzilla',
                'Show files missing Bugzilla component info')
    @CommandArgument('-r', '--rev',
                     help='Version control revision to look up info from')
    @CommandArgument('--format', choices={'json', 'plain'}, dest='fmt',
                     default='plain',
                     help='Output format')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_missing_bugzilla(self, paths, rev=None, fmt=None):
        missing = set()

        try:
            for p, m in self._get_files_info(paths, rev=rev).items():
                if 'BUG_COMPONENT' not in m:
                    missing.add(p)
        except InvalidPathException as e:
            print(e.message)
            return 1

        if fmt == 'json':
            json.dump({'missing': sorted(missing)}, sys.stdout,
                      indent=2)
            return
        elif fmt == 'plain':
            for f in sorted(missing):
                print(f)
        else:
            print('unhandled output format: %s' % fmt)
            return 1

    @SubCommand('file-info', 'bugzilla-automation',
                'Perform Bugzilla metadata analysis as required for automation')
    @CommandArgument('out_dir', help='Where to write files')
    def bugzilla_automation(self, out_dir):
        """Analyze and validate Bugzilla metadata as required by automation.

        This will write out JSON and gzipped JSON files for Bugzilla metadata.

        The exit code will be non-0 if Bugzilla metadata fails validation.
        """
        import gzip

        missing_component = set()
        seen_components = set()
        component_by_path = {}

        # TODO operate in VCS space. This requires teaching the VCS reader
        # to understand wildcards and/or for the relative path issue in the
        # VCS finder to be worked out.
        for p, m in sorted(self._get_files_info(['**']).items()):
            if 'BUG_COMPONENT' not in m:
                missing_component.add(p)
                print('Missing Bugzilla component: %s' % p)
                continue

            c = m['BUG_COMPONENT']
            seen_components.add(c)
            component_by_path[p] = [c.product, c.component]

        print('Examined %d files' % len(component_by_path))

        # We also have a normalized versions of the file to components mapping
        # that requires far less storage space by eliminating redundant strings.
        indexed_components = {i: [c.product, c.component]
                              for i, c in enumerate(sorted(seen_components))}
        components_index = {tuple(v): k for k, v in indexed_components.items()}
        normalized_component = {
            'components': indexed_components,
            'paths': {}
        }

        for p, c in component_by_path.items():
            d = normalized_component['paths']
            while '/' in p:
                base, p = p.split('/', 1)
                d = d.setdefault(base, {})

            d[p] = components_index[tuple(c)]

        if not os.path.exists(out_dir):
            os.makedirs(out_dir)

        components_json = os.path.join(out_dir, 'components.json')
        print('Writing %s' % components_json)
        with open(components_json, 'wb') as fh:
            json.dump(component_by_path, fh, sort_keys=True, indent=2)

        missing_json = os.path.join(out_dir, 'missing.json')
        print('Writing %s' % missing_json)
        with open(missing_json, 'wb') as fh:
            json.dump({'missing': sorted(missing_component)}, fh, indent=2)

        indexed_components_json = os.path.join(out_dir,
                                               'components-normalized.json')
        print('Writing %s' % indexed_components_json)
        with open(indexed_components_json, 'wb') as fh:
            # Don't indent so file is as small as possible.
            json.dump(normalized_component, fh, sort_keys=True)

        # Write compressed versions of JSON files.
        for p in (components_json, indexed_components_json, missing_json):
            gzip_path = '%s.gz' % p
            print('Writing %s' % gzip_path)
            with open(p, 'rb') as ifh, gzip.open(gzip_path, 'wb') as ofh:
                while True:
                    data = ifh.read(32768)
                    if not data:
                        break
                    ofh.write(data)

        # Causes CI task to fail if files are missing Bugzilla annotation.
        if missing_component:
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


    def _get_files_info(self, paths, rev=None):
        reader = self.mozbuild_reader(config_mode='empty', vcs_revision=rev)

        # Normalize to relative from topsrcdir.
        relpaths = []
        for p in paths:
            a = mozpath.abspath(p)
            if not mozpath.basedir(a, [self.topsrcdir]):
                raise InvalidPathException('path is outside topsrcdir: %s' % p)

            relpaths.append(mozpath.relpath(a, self.topsrcdir))

        # Expand wildcards.
        # One variable is for ordering. The other for membership tests.
        # (Membership testing on a list can be slow.)
        allpaths = []
        all_paths_set = set()
        for p in relpaths:
            if '*' not in p:
                if p not in all_paths_set:
                    if not os.path.exists(mozpath.join(self.topsrcdir, p)):
                        print('(%s does not exist; ignoring)' % p,
                              file=sys.stderr)
                        continue

                    all_paths_set.add(p)
                    allpaths.append(p)
                continue

            if rev:
                raise InvalidPathException('cannot use wildcard in version control mode')

            # finder is rooted at / for now.
            # TODO bug 1171069 tracks changing to relative.
            search = mozpath.join(self.topsrcdir, p)[1:]
            for path, f in reader.finder.find(search):
                path = path[len(self.topsrcdir):]
                if path not in all_paths_set:
                    all_paths_set.add(path)
                    allpaths.append(path)

        return reader.files_info(allpaths)


    @SubCommand('file-info', 'schedules',
                'Show the combined SCHEDULES for the files listed.')
    @CommandArgument('paths', nargs='+',
                     help='Paths whose data to query')
    def file_info_schedules(self, paths):
        """Show what is scheduled by the given files.

        Given a requested set of files (which can be specified using
        wildcards), print the total set of scheduled components.
        """
        from mozbuild.frontend.reader import EmptyConfig, BuildReader
        config = EmptyConfig(TOPSRCDIR)
        reader = BuildReader(config)
        schedules = set()
        for p, m in reader.files_info(paths).items():
            schedules |= set(m['SCHEDULES'].components)

        print(", ".join(schedules))
