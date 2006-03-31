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
# The Original Code is python code to drive jssh-enabled apps.
#
# The Initial Developer of the Original Code is
# davel@mozilla.com.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Grig Gheorghiu <grig@gheorghiu.net>
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
import unittest
import sys, os
import re, glob
import difflib

this_dir = os.path.abspath(os.path.dirname(__file__))
data_dir = os.path.join(this_dir, 'data')

from jssh_driver import JsshTester

class LayoutEngineTests(unittest.TestCase):
    
    def setUp(self):
        self.tester = JsshTester("localhost")
        self.get_files_from_data_dir()

    def get_files_from_data_dir(self):
        self.html_files = glob.glob("%s/*.html" % data_dir)
        print self.html_files
        self.golden_images = [re.sub("\.html$", "_inner_html.txt", filename) for filename in self.html_files]
        print self.golden_images
        
    def test_inner_html(self):
        self.failUnless(len(self.html_files) == len(self.golden_images))
        failures = 0
        for file, golden_image in zip(self.html_files, self.golden_images):
            local_url = "file:///" + file
            local_url = re.sub(r"\\", "/", local_url)
            print local_url
            inner_html = self.tester.get_innerHTML_from_URL(local_url)
            golden_inner_html = open(golden_image).read()
            if inner_html != golden_inner_html:
                failures += 1
        self.failUnless(failures == 0)

def main():
    unittest.main()

if __name__ == '__main__':
    main()
