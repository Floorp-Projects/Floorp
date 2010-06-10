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

import re
import sys

class UpdateChannel(object):

    # List of available update channels
    channels = ["betatest", "beta", "nightly", "releasetest", "release"]

    # Check if it's a valid update channel
    def isValidChannel(self, channel):
        try:
            self.channels.index(channel);
            return True
        except:
            return False

    # Get the current update channel
    def getChannel(self):
        try:
            file = open(self.getPrefFolder(), "r")
        except IOError, e:
            raise e
        else:
            content = file.read()
            file.close()

            result = re.search(r"(" + '|'.join(self.channels) + ")", content)
            return result.group(0)

    # Set the update channel to the specified one
    def setChannel(self, channel):
        print "Setting update channel to '%s'..." % channel

        if not self.isValidChannel(channel):
            raise Exception("%s is not a valid update channel" % channel)

        try:
            file = open(self.getPrefFolder(), "r")
        except IOError, e:
            raise e
        else:
            # Replace the current update channel with the specified one
            content = file.read()
            file.close()

            # Replace the current channel with the specified one
            result = re.sub(r"(" + '|'.join(self.channels) + ")",
                            channel, content)

            try:
                file = open(self.getPrefFolder(), "w")
            except IOError, e:
                raise e
            else:
                file.write(result)
                file.close()

                # Check that the correct channel has been set
                if channel != self.getChannel():
                    raise Exception("Update channel wasn't set correctly.")

    # Get the application's folder
    def getFolder(self):
        return self.folder

    # Set the application's folder
    def setFolder(self, folder):
        self.folder = folder

    # Get the default preferences folder
    def getPrefFolder(self):
        if sys.platform == "darwin":
            return self.folder + "/Contents/MacOS/defaults/pref/channel-prefs.js"
        elif sys.platform == "linux2":
            return self.folder + "/defaults/pref/channel-prefs.js"
        elif sys.platform == "win32":
            return self.folder  + "\\defaults\\pref\\channel-prefs.js"
