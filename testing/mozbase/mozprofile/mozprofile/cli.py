# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozprofile command line interface.
#
# The Initial Developer of the Original Code is
#   The Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mikeal Rogers <mikeal.rogers@gmail.com>
#   Clint Talbert <ctalbert@mozilla.com>
#   Jeff Hammel <jhammel@mozilla.com>
#   Andrew Halberstadt <halbersa@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

"""
Creates and/or modifies a Firefox profile.
The profile can be modified by passing in addons to install or preferences to set.
If no profile is specified, a new profile is created and the path of the resulting profile is printed.
"""

import sys
from addons import AddonManager
from optparse import OptionParser
from prefs import Preferences
from profile import Profile

__all__ = ['MozProfileCLI', 'cli']

class MozProfileCLI(object):

    module = 'mozprofile'

    def __init__(self, args=sys.argv[1:]):
        self.parser = OptionParser(description=__doc__)
        self.add_options(self.parser)
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


def cli(args=sys.argv[1:]):

    # process the command line
    cli = MozProfileCLI(args)

    # create the profile
    kwargs = cli.profile_args()
    kwargs['restore'] = False
    profile = Profile(**kwargs)

    # if no profile was passed in print the newly created profile
    if not cli.options.profile:
        print profile.profile

if __name__ == '__main__':
    cli()
