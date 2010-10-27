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
# The Original Code is Mozilla Corporation Code.
#
# The Initial Developer of the Original Code is the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2010
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Henrik Skupin <hskupin@gmail.com>
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

import os
import subprocess
import urllib

abs_path = os.path.dirname(os.path.abspath(__file__))
root_path = os.path.dirname(abs_path)

# We need the latest version of Event Utils
externalModules = [
    {   # EventUtils.js
        "url": "http://hg.mozilla.org/mozilla-central/raw-file/default/testing/mochitest/tests/SimpleTest/EventUtils.js",
        "path": "mozmill/extension/resource/stdlib/EventUtils.js",
        "patch": "patches/eventUtils.patch"
    },
    {   # httpd.js
        "url": "http://hg.mozilla.org/mozilla-central/raw-file/default/netwerk/test/httpserver/httpd.js",
        "path": "mozmill/extension/resource/stdlib/httpd.js",
        "patch": "patches/httpd.patch"
    }
]


# Change into the root folder to update and patch external modules correctly
os.chdir(root_path)

for module in externalModules:
    # Retrieve latest EventUtils module and apply our patch
    print "Downloading %s..." % (module["url"])
    urllib.urlretrieve (module["url"], os.path.join(root_path, module["path"]))

    print "Patching %s..." % (module["patch"])
    subprocess.call(["git", "apply", module["patch"]])
