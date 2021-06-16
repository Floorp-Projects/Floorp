# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
from __future__ import absolute_import
from __future__ import unicode_literals

import argparse
import os

from compare_locales.lint.linter import L10nLinter
from compare_locales.lint.util import (
    default_reference_and_tests,
    mirror_reference_and_tests,
    l10n_base_reference_and_tests,
)
from compare_locales import mozpath
from compare_locales import paths
from compare_locales import parser
from compare_locales import version


epilog = '''\
moz-l10n-lint checks for common mistakes in localizable files. It tests for
duplicate entries, parsing errors, and the like. Optionally, it can compare
the strings to an external reference with strings and warn if a string might
need to get a new ID.
'''


def main():
    p = argparse.ArgumentParser(
        description='Validate localizable strings',
        epilog=epilog,
    )
    p.add_argument('l10n_toml')
    p.add_argument(
        '--version', action='version', version='%(prog)s ' + version
    )
    p.add_argument('-W', action='store_true', help='error on warnings')
    p.add_argument(
        '--l10n-reference',
        dest='l10n_reference',
        metavar='PATH',
        help='check for conflicts against an l10n-only reference repository '
        'like gecko-strings',
    )
    p.add_argument(
        '--reference-project',
        dest='ref_project',
        metavar='PATH',
        help='check for conflicts against a reference project like '
        'android-l10n',
    )
    args = p.parse_args()
    if args.l10n_reference:
        l10n_base, locale = \
            os.path.split(os.path.abspath(args.l10n_reference))
        if not locale or not os.path.isdir(args.l10n_reference):
            p.error('Pass an existing l10n reference')
    else:
        l10n_base = '.'
        locale = None
    pc = paths.TOMLParser().parse(args.l10n_toml, env={'l10n_base': l10n_base})
    if locale:
        pc.set_locales([locale], deep=True)
    files = paths.ProjectFiles(locale, [pc])
    get_reference_and_tests = default_reference_and_tests
    if args.l10n_reference:
        get_reference_and_tests = l10n_base_reference_and_tests(files)
    elif args.ref_project:
        get_reference_and_tests = mirror_reference_and_tests(
            files, args.ref_project
        )
    linter = L10nLinter()
    results = linter.lint(
        (f for f, _, _, _ in files.iter_reference() if parser.hasParser(f)),
        get_reference_and_tests
    )
    rv = 0
    if results:
        rv = 1
        if all(r['level'] == 'warning' for r in results) and not args.W:
            rv = 0
    for result in results:
        print('{} ({}:{}): {}'.format(
            mozpath.relpath(result['path'], '.'),
            result.get('lineno', 0),
            result.get('column', 0),
            result['message']
        ))
    return rv


if __name__ == '__main__':
    main()
