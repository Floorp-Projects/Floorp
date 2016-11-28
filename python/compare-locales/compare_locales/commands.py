# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Commands exposed to commandlines'

import logging
from optparse import OptionParser, make_option

from compare_locales.paths import EnumerateApp
from compare_locales.compare import compareApp, compareDirs
from compare_locales.webapps import compare_web_app


class BaseCommand(object):
    """Base class for compare-locales commands.
    This handles command line parsing, and general sugar for setuptools
    entry_points.
    """
    options = [
        make_option('-v', '--verbose', action='count', dest='v', default=0,
                    help='Make more noise'),
        make_option('-q', '--quiet', action='count', dest='q', default=0,
                    help='Make less noise'),
        make_option('-m', '--merge',
                    help='''Use this directory to stage merged files,
use {ab_CD} to specify a different directory for each locale'''),
    ]
    data_option = make_option('--data', choices=['text', 'exhibit', 'json'],
                              default='text',
                              help='''Choose data and format (one of text,
exhibit, json); text: (default) Show which files miss which strings, together
with warnings and errors. Also prints a summary; json: Serialize the internal
tree, useful for tools. Also always succeeds; exhibit: Serialize the summary
data in a json useful for Exhibit
''')

    def __init__(self):
        self.parser = None

    def get_parser(self):
        """Get an OptionParser, with class docstring as usage, and
        self.options.
        """
        parser = OptionParser()
        parser.set_usage(self.__doc__)
        for option in self.options:
            parser.add_option(option)
        return parser

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
        (options, args) = self.parser.parse_args()
        # log as verbose or quiet as we want, warn by default
        logging.basicConfig()
        logging.getLogger().setLevel(logging.WARNING -
                                     (options.v - options.q)*10)
        observer = self.handle(args, options)
        print observer.serialize(type=options.data).encode('utf-8', 'replace')

    def handle(self, args, options):
        """Subclasses need to implement this method for the actual
        command handling.
        """
        raise NotImplementedError


class CompareLocales(BaseCommand):
    """usage: %prog [options] l10n.ini l10n_base_dir [locale ...]

Check the localization status of a gecko application.
The first argument is a path to the l10n.ini file for the application,
followed by the base directory of the localization repositories.
Then you pass in the list of locale codes you want to compare. If there are
not locales given, the list of locales will be taken from the all-locales file
of the application\'s l10n.ini."""

    options = BaseCommand.options + [
        make_option('--clobber-merge', action="store_true", default=False,
                    dest='clobber',
                    help="""WARNING: DATALOSS.
Use this option with care. If specified, the merge directory will
be clobbered for each module. That means, the subdirectory will
be completely removed, any files that were there are lost.
Be careful to specify the right merge directory when using this option."""),
        make_option('-r', '--reference', default='en-US', dest='reference',
                    help='Explicitly set the reference '
                    'localization. [default: en-US]'),
        BaseCommand.data_option
    ]

    def handle(self, args, options):
        if len(args) < 2:
            self.parser.error('Need to pass in list of languages')
        inipath, l10nbase = args[:2]
        locales = args[2:]
        app = EnumerateApp(inipath, l10nbase, locales)
        app.reference = options.reference
        try:
            observer = compareApp(app, merge_stage=options.merge,
                                  clobber=options.clobber)
        except (OSError, IOError), exc:
            print "FAIL: " + str(exc)
            self.parser.exit(2)
        return observer


class CompareDirs(BaseCommand):
    """usage: %prog [options] reference localization

Check the localization status of a directory tree.
The first argument is a path to the reference data,the second is the
localization to be tested."""

    options = BaseCommand.options + [
        BaseCommand.data_option
    ]

    def handle(self, args, options):
        if len(args) != 2:
            self.parser.error('Reference and localizatino required')
        reference, locale = args
        observer = compareDirs(reference, locale, merge_stage=options.merge)
        return observer


class CompareWebApp(BaseCommand):
    """usage: %prog [options] webapp [locale locale]

Check the localization status of a gaia-style web app.
The first argument is the directory of the web app.
Following arguments explicitly state the locales to test.
If none are given, test all locales in manifest.webapp or files."""

    options = BaseCommand.options[:-1] + [
        BaseCommand.data_option]

    def handle(self, args, options):
        if len(args) < 1:
            self.parser.error('Webapp directory required')
        basedir = args[0]
        locales = args[1:]
        observer = compare_web_app(basedir, locales)
        return observer
