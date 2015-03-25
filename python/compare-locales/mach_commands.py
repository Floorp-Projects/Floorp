# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function, unicode_literals

from mach.decorators import (
    CommandArgument,
    CommandProvider,
    Command,
)

from mozbuild.base import (
    MachCommandBase,
)

import mozpack


MERGE_HELP = '''Directory to merge to. Will be removed to before running
the comparison. Default: $(OBJDIR)/($MOZ_BUILD_APP)/locales/merge-$(AB_CD)
'''.lstrip()


@CommandProvider
class CompareLocales(MachCommandBase):
    """Run compare-locales."""

    @Command('compare-locales', category='testing',
             description='Run source checks on a localization.')
    @CommandArgument('--l10n-ini',
                     help='l10n.ini describing the app. ' +
                     'Default: $(MOZ_BUILD_APP)/locales/l10n.ini')
    @CommandArgument('--l10n-base',
                     help='Directory with the localizations. ' +
                     'Default: $(L10NBASEDIR)')
    @CommandArgument('--merge-dir',
                     help=MERGE_HELP)
    @CommandArgument('locales', nargs='+', metavar='ab_CD',
                     help='Locale codes to compare')
    def compare(self, l10n_ini=None, l10n_base=None, merge_dir=None,
                locales=None):
        from compare_locales.paths import EnumerateApp
        from compare_locales.compare import compareApp

        # check if we're configured and use defaults from there
        # otherwise, error early
        try:
            self.substs  # explicitly check
            if not l10n_ini:
                l10n_ini = mozpack.path.join(
                    self.topsrcdir,
                    self.substs['MOZ_BUILD_APP'],
                    'locales', 'l10n.ini'
                )
            if not l10n_base:
                l10n_base = mozpack.path.join(
                    self.topsrcdir,
                    self.substs['L10NBASEDIR']
                )
        except Exception:
            if not l10n_ini or not l10n_base:
                print('Specify --l10n-ini and --l10n-base or run configure.')
                return 1

        if not merge_dir:
            try:
                # self.substs is raising an Exception if we're not configured
                # don't merge if we're not
                merge_dir = mozpack.path.join(
                    self.topobjdir,
                    self.substs['MOZ_BUILD_APP'],
                    'locales', 'merge-{ab_CD}'
                )
            except Exception:
                pass

        app = EnumerateApp(l10n_ini, l10n_base, locales)
        observer = compareApp(app, merge_stage=merge_dir,
                              clobber=True)
        print(observer.serialize().encode('utf-8', 'replace'))
