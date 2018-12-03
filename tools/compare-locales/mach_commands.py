# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mach.base import (
    FailedCommandError,
)


@CommandProvider
class CompareLocales(object):
    """Run compare-locales."""

    @Command('compare-locales', category='build',
             description='Run source checks on a localization.')
    @CommandArgument('config_paths', metavar='l10n.toml', nargs='+',
                     help='TOML or INI file for the project')
    @CommandArgument('l10n_base_dir', metavar='l10n-base-dir',
                     help='Parent directory of localizations')
    @CommandArgument('locales', nargs='*', metavar='locale-code',
                     help='Locale code and top-level directory of '
                          'each localization')
    @CommandArgument('-q', '--quiet', action='count',
                     default=0, help='''Show less data.
Specified once, don't show obsolete entities. Specified twice, also hide
missing entities. Specify thrice to exclude warnings and four times to
just show stats''')
    @CommandArgument('-m', '--merge',
                     help='''Use this directory to stage merged files''')
    @CommandArgument('--validate', action='store_true',
                     help='Run compare-locales against reference')
    @CommandArgument('--json',
                     help='''Serialize to JSON. Value is the name of
the output file, pass "-" to serialize to stdout and hide the default output.
''')
    @CommandArgument('-D', action='append', metavar='var=value',
                     default=[], dest='defines',
                     help='Overwrite variables in TOML files')
    @CommandArgument('--full', action="store_true",
                     help="Compare projects that are disabled")
    @CommandArgument('--return-zero', action="store_true",
                     help="Return 0 regardless of l10n status")
    def compare(self, **kwargs):
        from compare_locales.commands import CompareLocales

        class ErrorHelper(object):
            '''Dummy ArgumentParser to marshall compare-locales
            commandline errors to mach exceptions.
            '''
            def error(self, msg):
                raise FailedCommandError(msg)

            def exit(self, message=None, status=0):
                raise FailedCommandError(message, exit_code=status)

        cmd = CompareLocales()
        cmd.parser = ErrorHelper()
        return cmd.handle(**kwargs)
