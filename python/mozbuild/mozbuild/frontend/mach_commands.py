# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import textwrap

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command
)

from mozbuild.frontend.sandbox_symbols import (
    FUNCTIONS,
    SPECIAL_VARIABLES,
    VARIABLES,
    doc_to_paragraphs,
)


def get_doc(doc):
    """Split documentation into summary line and everything else."""
    paragraphs = doc_to_paragraphs(doc)

    summary = paragraphs[0]
    extra = paragraphs[1:]

    return summary, extra

def print_extra(extra):
    """Prints the 'everything else' part of documentation intelligently."""
    for para in extra:
        for line in textwrap.wrap(para):
            print(line)

        print('')

    if not len(extra):
        print('')


@CommandProvider
class MozbuildFileCommands(object):
    @Command('mozbuild-reference', category='build-dev',
        description='View reference documentation on mozbuild files.')
    @CommandArgument('symbol', default=None, nargs='*',
        help='Symbol to view help on. If not specified, all will be shown.')
    @CommandArgument('--name-only', '-n', default=False, action='store_true',
        help='Print symbol names only.')
    def reference(self, symbol, name_only=False):
        if name_only:
            for s in sorted(VARIABLES.keys()):
                print(s)

            for s in sorted(FUNCTIONS.keys()):
                print(s)

            for s in sorted(SPECIAL_VARIABLES.keys()):
                print(s)

            return 0

        if len(symbol):
            for s in symbol:
                if s in VARIABLES:
                    self.variable_reference(s)
                    continue
                elif s in FUNCTIONS:
                    self.function_reference(s)
                    continue
                elif s in SPECIAL_VARIABLES:
                    self.special_reference(s)
                    continue

                print('Could not find symbol: %s' % s)
                return 1

            return 0

        print('=========')
        print('VARIABLES')
        print('=========')
        print('')
        print('This section lists all the variables that may be set ')
        print('in moz.build files.')
        print('')

        for v in sorted(VARIABLES.keys()):
            self.variable_reference(v)

        print('=========')
        print('FUNCTIONS')
        print('=========')
        print('')
        print('This section lists all the functions that may be called ')
        print('in moz.build files.')
        print('')

        for f in sorted(FUNCTIONS.keys()):
            self.function_reference(f)

        print('=================')
        print('SPECIAL VARIABLES')
        print('=================')
        print('')

        for v in sorted(SPECIAL_VARIABLES.keys()):
            self.special_reference(v)

        return 0

    def variable_reference(self, v):
        st_typ, in_type, default, doc = VARIABLES[v]

        print(v)
        print('=' * len(v))
        print('')

        summary, extra = get_doc(doc)

        print(summary)
        print('')
        print('Storage Type: %s' % st_typ.__name__)
        print('Input Type: %s' % in_type.__name__)
        print('Default Value: %s' % default)
        print('')
        print_extra(extra)

    def function_reference(self, f):
        attr, args, doc = FUNCTIONS[f]

        print(f)
        print('=' * len(f))
        print('')

        summary, extra = get_doc(doc)

        print(summary)
        print('')

        arg_types = []

        for t in args:
            if isinstance(t, list):
                inner_types = [t2.__name__ for t2 in t]
                arg_types.append(' | ' .join(inner_types))
                continue

            arg_types.append(t.__name__)

        arg_s = '(%s)' % ', '.join(arg_types)

        print('Arguments: %s' % arg_s)
        print('')
        print_extra(extra)

    def special_reference(self, v):
        typ, doc = SPECIAL_VARIABLES[v]

        print(v)
        print('=' * len(v))
        print('')

        summary, extra = get_doc(doc)

        print(summary)
        print('')
        print('Type: %s' % typ.__name__)
        print('')
        print_extra(extra)
