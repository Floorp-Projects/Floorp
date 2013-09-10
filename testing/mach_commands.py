# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import MachCommandBase


HANDLE_FILE_ERROR = '''
%s is a file.
However, I do not yet know how to run tests from files. You'll need to run
the mach command for the test type you are trying to run. e.g.
$ mach xpcshell-test path/to/file
'''.strip()

HANDLE_DIR_ERROR = '''
%s is a directory.
However, I do not yet know how to run tests from directories. You can try
running the mach command for the tests in that directory. e.g.
$ mach xpcshell-test path/to/directory
'''.strip()

UNKNOWN_TEST = '''
I don't know how to run the test: %s

You need to specify a test suite name or abbreviation. It's possible I just
haven't been told about this test suite yet. If you suspect that's the issue,
please file a bug at https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=General
and request support be added.
'''.strip()

MOCHITEST_CHUNK_BY_DIR = 4
MOCHITEST_TOTAL_CHUNKS = 5

TEST_SUITES = {
    'crashtest': {
        'aliases': ('C', 'Rc', 'RC', 'rc'),
        'mach_command': 'crashtest',
        'kwargs': {'test_file': None},
    },
    'crashtest-ipc': {
        'aliases': ('Cipc', 'cipc'),
        'mach_command': 'crashtest-ipc',
        'kwargs': {'test_file': None},
    },
    'jetpack': {
        'aliases': ('J',),
        'mach_command': 'jetpack-test',
        'kwargs': {},
    },
    'mochitest-a11y': {
        'mach_command': 'mochitest-a11y',
        'kwargs': {'test_file': None},
    },
    'mochitest-browser': {
        'aliases': ('bc', 'BC', 'Bc'),
        'mach_command': 'mochitest-browser',
        'kwargs': {'test_file': None},
    },
    'mochitest-chrome': {
        'mach_command': 'mochitest-chrome',
        'kwargs': {'test_file': None},
    },
    'mochitest-ipcplugins': {
        'make_target': 'mochitest-ipcplugins',
    },
    'mochitest-plain': {
        'mach_command': 'mochitest-plain',
        'kwargs': {'test_file': None},
    },
    'reftest': {
        'aliases': ('RR', 'rr', 'Rr'),
        'mach_command': 'reftest',
        'kwargs': {'test_file': None},
    },
    'reftest-ipc': {
        'aliases': ('Ripc',),
        'mach_command': 'reftest-ipc',
        'kwargs': {'test_file': None},
    },
    'xpcshell': {
        'aliases': ('X', 'x'),
        'mach_command': 'xpcshell-test',
        'kwargs': {'test_file': 'all'},
    },
}

for i in range(1, MOCHITEST_TOTAL_CHUNKS + 1):
    TEST_SUITES['mochitest-%d' %i] = {
        'aliases': ('M%d' % i, 'm%d' % i),
        'mach_command': 'mochitest-plain',
        'kwargs': {
            'chunk_by_dir': MOCHITEST_CHUNK_BY_DIR,
            'total_chunks': MOCHITEST_TOTAL_CHUNKS,
            'this_chunk': i,
            'test_file': None,
        },
    }

TEST_HELP = '''
Test or tests to run. Tests can be specified by test suite name or alias.
The following test suites and aliases are supported: %s
''' % ', '.join(sorted(TEST_SUITES))
TEST_HELP = TEST_HELP.strip()


@CommandProvider
class Test(MachCommandBase):
    @Command('test', category='testing', description='Run tests.')
    @CommandArgument('what', default=None, nargs='*', help=TEST_HELP)
    def test(self, what):
        status = None
        for entry in what:
            status = self._run_test(entry)

            if status:
                break

        return status

    def _run_test(self, what):
        suite = None
        if what in TEST_SUITES:
            suite = TEST_SUITES[what]
        else:
            for v in TEST_SUITES.values():
                if what in v.get('aliases', []):
                    suite = v
                    break

        if suite:
            if 'mach_command' in suite:
                return self._mach_context.commands.dispatch(
                    suite['mach_command'], self._mach_context, **suite['kwargs'])

            if 'make_target' in suite:
                return self._run_make(target=suite['make_target'],
                    pass_thru=True)

            raise Exception('Do not know how to run suite. This is a logic '
                'error in this mach command.')

        if os.path.isfile(what):
            print(HANDLE_FILE_ERROR % what)
            return 1

        if os.path.isdir(what):
            print(HANDLE_DIR_ERROR % what)
            return 1

        print(UNKNOWN_TEST % what)
        return 1
