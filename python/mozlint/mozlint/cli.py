# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
from argparse import REMAINDER, ArgumentParser

from mozlint.formatters import all_formatters

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
        [['--list'],
         {'dest': 'list_linters',
          'default': False,
          'action': 'store_true',
          'help': "List all available linters and exit.",
          }],
        [['-W', '--warnings'],
         {'dest': 'show_warnings',
          'default': False,
          'action': 'store_true',
          'help': "Display and fail on warnings in addition to errors.",
          }],
        [['-f', '--format'],
         {'dest': 'formats',
          'action': 'append',
          'help': "Formatter to use. Defaults to 'stylish' on stdout. "
                  "You can specify an optional path as --format formatter:path "
                  "that will be used instead of stdout. "
                  "You can also use multiple formatters at the same time. "
                  "Formatters available: {}.".format(', '.join(all_formatters.keys())),
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
        [['--fix'],
         {'action': 'store_true',
          'default': False,
          'help': "Fix lint errors if possible. Any errors that could not be fixed "
                  "will be printed as normal."
          }],
        [['--edit'],
         {'action': 'store_true',
          'default': False,
          'help': "Each file containing lint errors will be opened in $EDITOR one after "
                  "the other."
          }],
        [['--setup'],
         {'action': 'store_true',
          'default': False,
          'help': "Bootstrap linter dependencies without running any of the linters."
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

        self.validate(args)
        return args, extra

    def validate(self, args):
        if args.edit and not os.environ.get('EDITOR'):
            self.error("must set the $EDITOR environment variable to use --edit")

        if args.paths:
            invalid = [p for p in args.paths if not os.path.exists(p)]
            if invalid:
                self.error("the following paths do not exist:\n{}".format("\n".join(invalid)))

        if args.formats:
            formats = []
            for fmt in args.formats:
                path = None
                if ':' in fmt:
                    # Detect optional formatter path
                    pos = fmt.index(':')
                    fmt, path = fmt[:pos], os.path.realpath(fmt[pos+1:])

                    # Check path is writable
                    fmt_dir = os.path.dirname(path)
                    if not os.access(fmt_dir, os.W_OK | os.X_OK):
                        self.error("the following directory is not writable: {}".format(fmt_dir))

                if fmt not in all_formatters.keys():
                    self.error('the following formatter is not available: {}'.format(fmt))

                formats.append((fmt, path))
            args.formats = formats
        else:
            # Can't use argparse default or this choice will be always present
            args.formats = [('stylish', None)]


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


def run(paths, linters, formats, outgoing, workdir, edit,
        setup=False, list_linters=False, **lintargs):
    from mozlint import LintRoller, formatters
    from mozlint.editor import edit_issues

    if list_linters:
        lint_paths = find_linters(linters)
        print("Available linters: {}".format(
            [os.path.splitext(os.path.basename(l))[0] for l in lint_paths]
        ))
        return 0

    lint = LintRoller(**lintargs)
    lint.read(find_linters(linters))

    # Always run bootstrapping, but return early if --setup was passed in.
    ret = lint.setup()
    if setup:
        return ret

    # run all linters
    result = lint.roll(paths, outgoing=outgoing, workdir=workdir)

    if edit and result.issues:
        edit_issues(result)
        result = lint.roll(result.issues.keys())

    for formatter_name, path in formats:
        formatter = formatters.get(formatter_name)

        # Encode output with 'replace' to avoid UnicodeEncodeErrors on
        # environments that aren't using utf-8.
        out = formatter(result).encode(sys.stdout.encoding or 'ascii', 'replace')
        if out:
            output_file = open(path, 'w') if path else sys.stdout
            print(out, file=output_file)

    return result.returncode


if __name__ == '__main__':
    parser = MozlintParser()
    args = vars(parser.parse_args())
    sys.exit(run(**args))
