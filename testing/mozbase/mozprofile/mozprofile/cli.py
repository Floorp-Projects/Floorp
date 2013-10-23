#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

"""
Creates and/or modifies a Firefox profile.
The profile can be modified by passing in addons to install or preferences to set.
If no profile is specified, a new profile is created and the path of the resulting profile is printed.
"""

import sys
from addons import AddonManager
from optparse import OptionParser
from prefs import Preferences
from profile import FirefoxProfile
from profile import Profile

__all__ = ['MozProfileCLI', 'cli']

class MozProfileCLI(object):
    """The Command Line Interface for ``mozprofile``."""

    module = 'mozprofile'
    profile_class = Profile

    def __init__(self, args=sys.argv[1:], add_options=None):
        self.parser = OptionParser(description=__doc__)
        self.add_options(self.parser)
        if add_options:
            add_options(self.parser)
        (self.options, self.args) = self.parser.parse_args(args)

    def add_options(self, parser):

        parser.add_option("-p", "--profile", dest="profile",
                          help="The path to the profile to operate on. If none, creates a new profile in temp directory")
        parser.add_option("-a", "--addon", dest="addons",
                          action="append", default=[],
                          help="Addon paths to install. Can be a filepath, a directory containing addons, or a url")
        parser.add_option("--addon-manifests", dest="addon_manifests",
                          action="append",
                          help="An addon manifest to install")
        parser.add_option("--pref", dest="prefs",
                          action='append', default=[],
                          help="A preference to set. Must be a key-value pair separated by a ':'")
        parser.add_option("--preferences", dest="prefs_files",
                          action='append', default=[],
                          metavar="FILE",
                          help="read preferences from a JSON or INI file. For INI, use 'file.ini:section' to specify a particular section.")

    def profile_args(self):
        """arguments to instantiate the profile class"""
        return dict(profile=self.options.profile,
                    addons=self.options.addons,
                    addon_manifests=self.options.addon_manifests,
                    preferences=self.preferences())

    def preferences(self):
        """profile preferences"""

        # object to hold preferences
        prefs = Preferences()

        # add preferences files
        for prefs_file in self.options.prefs_files:
            prefs.add_file(prefs_file)

        # change CLI preferences into 2-tuples
        separator = ':'
        cli_prefs = []
        for pref in self.options.prefs:
            if separator not in pref:
                self.parser.error("Preference must be a key-value pair separated by a ':' (You gave: %s)" % pref)
            cli_prefs.append(pref.split(separator, 1))

        # string preferences
        prefs.add(cli_prefs, cast=True)

        return prefs()

    def profile(self, restore=False):
        """create the profile"""

        kwargs = self.profile_args()
        kwargs['restore'] = restore
        return self.profile_class(**kwargs)


def cli(args=sys.argv[1:]):
    """ Handles the command line arguments for ``mozprofile`` via ``sys.argv``"""

    # add a view method for this cli method only
    def add_options(parser):
        parser.add_option('--view', dest='view',
                          action='store_true', default=False,
                          help="view summary of profile following invocation")
        parser.add_option('--firefox', dest='firefox_profile',
                          action='store_true', default=False,
                          help="use FirefoxProfile defaults")

    # process the command line
    cli = MozProfileCLI(args, add_options)

    if cli.args:
        cli.parser.error("Program doesn't support positional arguments.")

    if cli.options.firefox_profile:
        cli.profile_class = FirefoxProfile

    # create the profile
    profile = cli.profile()

    if cli.options.view:
        # view the profile, if specified
        print profile.summary()
        return

    # if no profile was passed in print the newly created profile
    if not cli.options.profile:
        print profile.profile

if __name__ == '__main__':
    cli()
