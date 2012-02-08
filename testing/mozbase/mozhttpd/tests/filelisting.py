#!/usr/bin/env python

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
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is
# the Mozilla Foundation.
# Portions created by the Initial Developer are Copyright (C) 2011
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Joel Maher <joel.maher@gmail.com>
#   William Lachance <wlachance@mozilla.com>
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

import mozhttpd
import urllib2
import os
import unittest
import re

here = os.path.dirname(os.path.abspath(__file__))

class FileListingTest(unittest.TestCase):

    def check_filelisting(self, path=''):
        filelist = os.listdir(here)

        httpd = mozhttpd.MozHttpd(port=0, docroot=here)
        httpd.start(block=False)
        f = urllib2.urlopen("http://%s:%s/%s" % ('127.0.0.1', httpd.httpd.server_port, path))
        for line in f.readlines():
            webline = re.sub('\<[a-zA-Z0-9\-\_\.\=\"\'\/\\\%\!\@\#\$\^\&\*\(\) ]*\>', '', line.strip('\n')).strip('/').strip().strip('@')

            if webline and not webline.startswith("Directory listing for"):
                self.assertTrue(webline in filelist,
                                "File %s in dir listing corresponds to a file" % webline)
                filelist.remove(webline)
        self.assertFalse(filelist, "Should have no items in filelist (%s) unaccounted for" % filelist)


    def test_filelist(self):
        self.check_filelisting()

    def test_filelist_params(self):
        self.check_filelisting('?foo=bar&fleem=&foo=fleem')


if __name__ == '__main__':
    unittest.main()
