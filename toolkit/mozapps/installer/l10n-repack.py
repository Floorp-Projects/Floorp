# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'''
Replace localized parts of a packaged directory with data from a langpack
directory.
'''

from mozpack.packager import l10n
from argparse import ArgumentParser
import buildconfig

# Set of files or directories not listed in a chrome.manifest but that are
# localized.
NON_CHROME = set([
    '**/crashreporter*.ini',
    'dictionaries',
    'defaults/profile',
    'defaults/pref*/*-l10n.js',
    'locale.ini',
    'update.locale',
    'updater.ini',
    'extensions/langpack-*@*',
    'distribution/extensions/langpack-*@*',
    '**/multilocale.txt'
])


def valid_extra_l10n(arg):
    if '=' not in arg:
        raise ValueError('Invalid value')
    return tuple(arg.split('=', 1))


def main():
    parser = ArgumentParser()
    parser.add_argument('build',
                        help='Directory containing the build to repack')
    parser.add_argument('l10n',
                        help='Directory containing the staged langpack')
    parser.add_argument('extra_l10n', nargs='*', metavar='BASE=PATH',
                        type=valid_extra_l10n,
                        help='Extra directories with staged localized files '
                             'to be considered under the given base in the '
                             'repacked build')
    parser.add_argument('--non-resource', nargs='+', metavar='PATTERN',
                        default=[],
                        help='Extra files not to be considered as resources')
    args = parser.parse_args()

    buildconfig.substs['USE_ELF_HACK'] = False
    buildconfig.substs['PKG_SKIP_STRIP'] = True
    l10n.repack(args.build, args.l10n, extra_l10n=dict(args.extra_l10n),
                non_resources=args.non_resource, non_chrome=NON_CHROME)


if __name__ == "__main__":
    main()
