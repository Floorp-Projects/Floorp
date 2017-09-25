# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

'Commands exposed to commandlines'

import logging
from argparse import ArgumentParser
import os

from compare_locales import version
from compare_locales.paths import EnumerateApp, TOMLParser, ConfigNotFound
from compare_locales.compare import compareProjects, Observer


class CompareLocales(object):
    """Check the localization status of gecko applications.
The first arguments are paths to the l10n.ini or toml files for the
applications, followed by the base directory of the localization repositories.
Then you pass in the list of locale codes you want to compare. If there are
not locales given, the list of locales will be taken from the l10n.toml file
or the all-locales file referenced by the application\'s l10n.ini."""

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
        parser.add_argument('config_paths', metavar='l10n.toml', nargs='+',
                            help='TOML or INI file for the project')
        parser.add_argument('l10n_base_dir', metavar='l10n-base-dir',
                            help='Parent directory of localizations')
        parser.add_argument('locales', nargs='*', metavar='locale-code',
                            help='Locale code and top-level directory of '
                                 'each localization')
        parser.add_argument('-D', action='append', metavar='var=value',
                            default=[], dest='defines',
                            help='Overwrite variables in TOML files')
        parser.add_argument('--unified', action="store_true",
                            help="Show output for all projects unified")
        parser.add_argument('--full', action="store_true",
                            help="Compare projects that are disabled")
        parser.add_argument('--clobber-merge', action="store_true",
                            default=False, dest='clobber',
                            help="""WARNING: DATALOSS.
Use this option with care. If specified, the merge directory will
be clobbered for each module. That means, the subdirectory will
be completely removed, any files that were there are lost.
Be careful to specify the right merge directory when using this option.""")
        parser.add_argument('--data', choices=['text', 'exhibit', 'json'],
                            default='text',
                            help='''Choose data and format (one of text,
exhibit, json); text: (default) Show which files miss which strings, together
with warnings and errors. Also prints a summary; json: Serialize the internal
tree, useful for tools. Also always succeeds; exhibit: Serialize the summary
data in a json useful for Exhibit
''')
        return parser

    @classmethod
    def call(cls):
        """Entry_point for setuptools.
        The actual command handling is done in the handle() method of the
        subclasses.
        """
        cmd = cls()
        return cmd.handle_()

    def handle_(self):
        """The instance part of the classmethod call."""
        self.parser = self.get_parser()
        args = self.parser.parse_args()
        # log as verbose or quiet as we want, warn by default
        logging.basicConfig()
        logging.getLogger().setLevel(logging.WARNING -
                                     (args.v - args.q) * 10)
        kwargs = vars(args)
        # strip handeld arguments
        kwargs.pop('q')
        kwargs.pop('v')
        return self.handle(**kwargs)

    def handle(self, config_paths, l10n_base_dir, locales,
               merge=None, defines=None, unified=False, full=False,
               clobber=False, data='text'):
        # using nargs multiple times in argparser totally screws things
        # up, repair that.
        # First files are configs, then the base dir, everything else is
        # locales
        all_args = config_paths + [l10n_base_dir] + locales
        config_paths = []
        locales = []
        if defines is None:
            defines = []
        while all_args and not os.path.isdir(all_args[0]):
            config_paths.append(all_args.pop(0))
        if not config_paths:
            self.parser.error('no configuration file given')
        for cf in config_paths:
            if not os.path.isfile(cf):
                self.parser.error('config file %s not found' % cf)
        if not all_args:
            self.parser.error('l10n-base-dir not found')
        l10n_base_dir = all_args.pop(0)
        locales.extend(all_args)
        # when we compare disabled projects, we set our locales
        # on all subconfigs, so deep is True.
        locales_deep = full
        configs = []
        config_env = {}
        for define in defines:
            var, _, value = define.partition('=')
            config_env[var] = value
        for config_path in config_paths:
            if config_path.endswith('.toml'):
                try:
                    config = TOMLParser.parse(config_path, env=config_env)
                except ConfigNotFound as e:
                    self.parser.exit('config file %s not found' % e.filename)
                config.add_global_environment(l10n_base=l10n_base_dir)
                if locales:
                    config.set_locales(locales, deep=locales_deep)
                configs.append(config)
            else:
                app = EnumerateApp(
                    config_path, l10n_base_dir, locales)
                configs.append(app.asConfig())
        try:
            unified_observer = None
            if unified:
                unified_observer = Observer()
            observers = compareProjects(
                configs,
                stat_observer=unified_observer,
                merge_stage=merge, clobber_merge=clobber)
        except (OSError, IOError), exc:
            print "FAIL: " + str(exc)
            self.parser.exit(2)
        if unified:
            observers = [unified_observer]

        rv = 0
        for observer in observers:
            print observer.serialize(type=data)
            # summary is a dict of lang-summary dicts
            # find out if any of our results has errors, return 1 if so
            if rv > 0:
                continue  # we already have errors
            for loc, summary in observer.summary.items():
                if summary.get('errors', 0) > 0:
                    rv = 1
                    # no need to check further summaries, but
                    # continue to run through observers
                    break
        return rv
