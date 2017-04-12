# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Commands exposed to commandlines'

import logging
from argparse import ArgumentParser

from compare_locales import version
from compare_locales.paths import EnumerateApp
from compare_locales.compare import compareApp, compareDirs
from compare_locales.webapps import compare_web_app


class BaseCommand(object):
    """Base class for compare-locales commands.
    This handles command line parsing, and general sugar for setuptools
    entry_points.
    """

    def __init__(self):
        self.parser = None

    def get_parser(self):
        """Get an ArgumentParser, with class docstring as description.
        """
        parser = ArgumentParser(description=self.__doc__)
        parser.add_argument('--version', action='version',
                            version='%(prog)s ' + version)
        parser.add_argument('-v', '--verbose', action='count', dest='v',
                            default=0, help='Make more noise')
        parser.add_argument('-q', '--quiet', action='count', dest='q',
                            default=0, help='Make less noise')
        parser.add_argument('-m', '--merge',
                            help='''Use this directory to stage merged files,
use {ab_CD} to specify a different directory for each locale''')
        return parser

    def add_data_argument(self, parser):
        parser.add_argument('--data', choices=['text', 'exhibit', 'json'],
                            default='text',
                            help='''Choose data and format (one of text,
exhibit, json); text: (default) Show which files miss which strings, together
with warnings and errors. Also prints a summary; json: Serialize the internal
tree, useful for tools. Also always succeeds; exhibit: Serialize the summary
data in a json useful for Exhibit
''')

    @classmethod
    def call(cls):
        """Entry_point for setuptools.
        The actual command handling is done in the handle() method of the
        subclasses.
        """
        cmd = cls()
        cmd.handle_()

    def handle_(self):
        """The instance part of the classmethod call."""
        self.parser = self.get_parser()
        args = self.parser.parse_args()
        # log as verbose or quiet as we want, warn by default
        logging.basicConfig()
        logging.getLogger().setLevel(logging.WARNING -
                                     (args.v - args.q) * 10)
        observer = self.handle(args)
        print observer.serialize(type=args.data).encode('utf-8', 'replace')

    def handle(self, args):
        """Subclasses need to implement this method for the actual
        command handling.
        """
        raise NotImplementedError


class CompareLocales(BaseCommand):
    """Check the localization status of a gecko application.
The first argument is a path to the l10n.ini file for the application,
followed by the base directory of the localization repositories.
Then you pass in the list of locale codes you want to compare. If there are
not locales given, the list of locales will be taken from the all-locales file
of the application\'s l10n.ini."""

    def get_parser(self):
        parser = super(CompareLocales, self).get_parser()
        parser.add_argument('ini_file', metavar='l10n.ini',
                            help='INI file for the project')
        parser.add_argument('l10n_base_dir', metavar='l10n-base-dir',
                            help='Parent directory of localizations')
        parser.add_argument('locales', nargs='*', metavar='locale-code',
                            help='Locale code and top-level directory of '
                                 'each localization')
        parser.add_argument('--clobber-merge', action="store_true",
                            default=False, dest='clobber',
                            help="""WARNING: DATALOSS.
Use this option with care. If specified, the merge directory will
be clobbered for each module. That means, the subdirectory will
be completely removed, any files that were there are lost.
Be careful to specify the right merge directory when using this option.""")
        parser.add_argument('-r', '--reference', default='en-US',
                            dest='reference',
                            help='Explicitly set the reference '
                            'localization. [default: en-US]')
        self.add_data_argument(parser)
        return parser

    def handle(self, args):
        app = EnumerateApp(args.ini_file, args.l10n_base_dir, args.locales)
        app.reference = args.reference
        try:
            observer = compareApp(app, merge_stage=args.merge,
                                  clobber=args.clobber)
        except (OSError, IOError), exc:
            print "FAIL: " + str(exc)
            self.parser.exit(2)
        return observer


class CompareDirs(BaseCommand):
    """Check the localization status of a directory tree.
The first argument is a path to the reference data,the second is the
localization to be tested."""

    def get_parser(self):
        parser = super(CompareDirs, self).get_parser()
        parser.add_argument('reference')
        parser.add_argument('localization')
        self.add_data_argument(parser)
        return parser

    def handle(self, args):
        observer = compareDirs(args.reference, args.localization,
                               merge_stage=args.merge)
        return observer


class CompareWebApp(BaseCommand):
    """Check the localization status of a gaia-style web app.
The first argument is the directory of the web app.
Following arguments explicitly state the locales to test.
If none are given, test all locales in manifest.webapp or files."""

    def get_parser(self):
        parser = super(CompareWebApp, self).get_parser()
        parser.add_argument('webapp')
        parser.add_argument('locales', nargs='*', metavar='locale-code',
                            help='Locale code and top-level directory of '
                                 'each localization')
        self.add_data_argument(parser)
        return parser

    def handle(self, args):
        observer = compare_web_app(args.webapp, args.locales)
        return observer
