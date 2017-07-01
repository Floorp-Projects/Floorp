# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

import os
import sys
from argparse import REMAINDER, ArgumentParser

SEARCH_PATHS = []


class MozlintParser(ArgumentParser):
    arguments = [
        [['paths'],
         {'nargs': '*',
          'default': None,
          'help': "Paths to file or directories to lint, like "
                  "'browser/components/loop' or 'mobile/android'. "
                  "Defaults to the current directory if not given.",
          }],
        [['-l', '--linter'],
         {'dest': 'linters',
          'default': [],
          'action': 'append',
          'help': "Linters to run, e.g 'eslint'. By default all linters "
                  "are run for all the appropriate files.",
          }],
        [['-f', '--format'],
         {'dest': 'fmt',
          'default': 'stylish',
          'help': "Formatter to use. Defaults to 'stylish'.",
          }],
        [['-n', '--no-filter'],
         {'dest': 'use_filters',
          'default': True,
          'action': 'store_false',
          'help': "Ignore all filtering. This is useful for quickly "
                  "testing a directory that otherwise wouldn't be run, "
                  "without needing to modify the config file.",
          }],
        [['-o', '--outgoing'],
         {'const': 'default',
          'nargs': '?',
          'help': "Lint files touched by commits that are not on the remote repository. "
                  "Without arguments, finds the default remote that would be pushed to. "
                  "The remote branch can also be specified manually. Works with "
                  "mercurial or git."
          }],
        [['-w', '--workdir'],
         {'const': 'all',
          'nargs': '?',
          'choices': ['staged', 'all'],
          'help': "Lint files touched by changes in the working directory "
                  "(i.e haven't been committed yet). On git, --workdir=staged "
                  "can be used to only consider staged files. Works with "
                  "mercurial or git.",
          }],
        [['extra_args'],
         {'nargs': REMAINDER,
          'help': "Extra arguments that will be forwarded to the underlying linter.",
          }],
    ]

    def __init__(self, **kwargs):
        ArgumentParser.__init__(self, usage=self.__doc__, **kwargs)

        for cli, args in self.arguments:
            self.add_argument(*cli, **args)

    def parse_known_args(self, *args, **kwargs):
        # Allow '-wo' or '-ow' as shorthand for both --workdir and --outgoing.
        for token in ('-wo', '-ow'):
            if token in args[0]:
                i = args[0].index(token)
                args[0].pop(i)
                args[0][i:i] = [token[:2], '-' + token[2]]

        # This is here so the eslint mach command doesn't lose 'extra_args'
        # when using mach's dispatch functionality.
        args, extra = ArgumentParser.parse_known_args(self, *args, **kwargs)
        args.extra_args = extra
        return args, extra


def find_linters(linters=None):
    lints = []
    for search_path in SEARCH_PATHS:
        if not os.path.isdir(search_path):
            continue

        sys.path.insert(0, search_path)
        files = os.listdir(search_path)
        for f in files:
            name = os.path.basename(f)

            if not name.endswith('.yml'):
                continue

            name = name.rsplit('.', 1)[0]

            if linters and name not in linters:
                continue

            lints.append(os.path.join(search_path, f))
    return lints


def run(paths, linters, fmt, outgoing, workdir, **lintargs):
    from mozlint import LintRoller, formatters

    lint = LintRoller(**lintargs)
    lint.read(find_linters(linters))

    # run all linters
    results = lint.roll(paths, outgoing=outgoing, workdir=workdir)
    formatter = formatters.get(fmt)

    # Encode output with 'replace' to avoid UnicodeEncodeErrors on
    # environments that aren't using utf-8.
    print(formatter(results, failed=lint.failed).encode(
        sys.stdout.encoding or 'ascii', 'replace'))
    return 1 if results or lint.failed else 0


if __name__ == '__main__':
    parser = MozlintParser()
    args = vars(parser.parse_args())
    sys.exit(run(**args))
