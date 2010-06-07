#!/usr/bin/python

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
# The Original Code is MozMill Test code.
#
# The Initial Developer of the Original Code is Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2009
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Henrik Skupin <hskupin@mozilla.com>
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

import copy
import datetime
import mozmill
import optparse
import os
import sys

try:
    import json
except:
    import simplejson as json

abs_path = os.path.dirname(os.path.abspath(__file__))

# Import local libraries
sys.path.append(os.path.join(abs_path, "libs"))
from install import Installer
from prefs import UpdateChannel

class SoftwareUpdateCLI(mozmill.RestartCLI):
    app_binary = {'darwin' : '', 'linux2' : '/firefox', 'win32' : '/firefox.exe'}
    test_path = abs_path + "/../firefox/softwareUpdate/"

    # Parser options and arguments
    parser_options = copy.copy(mozmill.RestartCLI.parser_options)
    parser_options.pop(("-s", "--shell"))
    parser_options.pop(("-t", "--test",))
    parser_options.pop(("-u", "--usecode"))

    parser_options[("-c", "--channel",)] = dict(dest="channel", default=None, 
                                           help="Update channel (betatest, beta, nightly, releasetest, release)")
    parser_options[("--no-fallback",)] = dict(dest="no_fallback", default=None,
                                              action = "store_true",
                                              help="No fallback update should be performed")
    parser_options[("-t", "--type",)] = dict(dest="type", default="minor",
                                             nargs = 1,
                                             help="Type of the update (minor, major)")
    parser_options[("-i", "--install",)] = dict(dest="install", default=None,
                                                nargs = 1,
                                                help="Installation folder for the build")

    def __init__(self):
        super(SoftwareUpdateCLI, self).__init__()
        self.options.shell = None
        self.options.usecode = None
        self.options.plugins = None

        # We need at least one argument
        if len(self.args) < 1:
            print "No arguments specified. Please run with --help to see all options"
            sys.exit(1)

        # If a folder is given as argument it will be expanded to the contained
        # files. So we only have to specify the folder instead of all builds
        if self.options.install and os.path.isdir(self.args[0]):
            folder = self.args.pop(0)
            files = os.listdir(folder)

            if files is None: files = []
            for file in files:
                full_path = os.path.join(folder, file)
                if os.path.isfile(full_path):
                    self.args.append(full_path)

        # Check the type of the update and default to minor
        if self.options.type != "minor" and self.options.type != "major":
            self.options.type = "minor"

    def prepare_channel(self):
        channel = UpdateChannel()
        channel.setFolder(self.options.folder)

        if self.options.channel is None:
            self.channel = channel.getChannel()
        else:
            channel.setChannel(self.options.channel)
            self.channel = self.options.channel

    def prepare_build(self, binary):
        ''' Prepare the build for the test run '''
        if self.options.install is not None:
            self.options.folder = Installer().install(binary, self.options.install)
            self.options.binary = self.options.folder + self.app_binary[sys.platform]
        else:
            folder = os.path.dirname(binary)
            self.options.folder = folder if not os.path.isdir(binary) else binary
            self.options.binary = binary

    def cleanup_build(self):
        # Always remove the build when it has been installed
        if self.options.install:
            Installer().uninstall(self.options.folder)

    def build_wiki_entry(self, results):
        entry = "* %s => %s, %s, %s, %s, %s, %s, '''%s'''\n" \
                "** %s ID:%s\n** %s ID:%s\n" \
                "** Passed %d :: Failed %d :: Skipped %d\n" % \
                (results.get("preVersion", ""),
                 results.get("postVersion", ""),
                 results.get("type"),
                 results.get("preLocale", ""),
                 results.get("updateType", "unknown type"),
                 results.get("channel", ""),
                 datetime.date.today(),
                 "PASS" if results.get("success", False) else "FAIL",
                 results.get("preUserAgent", ""), results.get("preBuildId", ""),
                 results.get("postUserAgent", ""), results.get("postBuildId", ""),
                 len(results.get("passes")),
                 len(results.get("fails")),
                 len(results.get("skipped")))
        return entry

    def run_test(self, binary, is_fallback = False, *args, **kwargs):
        try:
            self.prepare_build(binary)
            self.prepare_channel()

            self.mozmill.passes = []
            self.mozmill.fails = []
            self.mozmill.skipped = []
            self.mozmill.alltests = []

            self.mozmill.persisted = {}
            self.mozmill.persisted["channel"] = self.channel
            self.mozmill.persisted["type"] = self.options.type

            if is_fallback:
                self.options.test = self.test_path + "testFallbackUpdate/"
            else:
                self.options.test = self.test_path + "testDirectUpdate/"

            super(SoftwareUpdateCLI, self)._run(*args, **kwargs)
        except Exception, e:
            print e

        self.cleanup_build()

        # If a Mozmill test fails the update will also fail
        if self.mozmill.fails:
            self.mozmill.persisted["success"] = False

        self.mozmill.persisted["passes"] = self.mozmill.passes
        self.mozmill.persisted["fails"] = self.mozmill.fails
        self.mozmill.persisted["skipped"] = self.mozmill.skipped

        return self.mozmill.persisted

    def run(self, *args, **kwargs):
        ''' Run software update tests for all specified builds '''

        # Run direct and fallback update tests for each build
        self.wiki = []
        for binary in self.args:
            direct = self.run_test(binary, False)
            result_direct = direct.get("success", False);

            if not self.options.no_fallback:
                fallback = self.run_test(binary, True)
                result_fallback = fallback.get("success", False)
            else:
                result_fallback = False

            if not (result_direct and result_fallback):
                self.wiki.append(self.build_wiki_entry(direct))
            if not self.options.no_fallback:
                self.wiki.append(self.build_wiki_entry(fallback))

        # Print results to the console
        print "\nResults:\n========"
        for result in self.wiki:
            print result

if __name__ == "__main__":
    SoftwareUpdateCLI().run()
