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
    'searchplugins',
    'dictionaries',
    'hyphenation',
    'defaults/profile',
    'defaults/pref*/*-l10n.js',
    'update.locale',
    'extensions/langpack-*@*',
    'distribution/extensions/langpack-*@*',
    'chrome/**/searchplugins/*.xml',
])


def main():
    parser = ArgumentParser()
    parser.add_argument('build',
                        help='Directory containing the build to repack')
    parser.add_argument('l10n',
                        help='Directory containing the staged langpack')
    parser.add_argument('--non-resource', nargs='+', metavar='PATTERN',
                        default=[],
                        help='Extra files not to be considered as resources')
    args = parser.parse_args()

    buildconfig.substs['USE_ELF_HACK'] = False
    buildconfig.substs['PKG_SKIP_STRIP'] = True
    l10n.repack(args.build, args.l10n, args.non_resource, NON_CHROME)


if __name__ == "__main__":
    main()
