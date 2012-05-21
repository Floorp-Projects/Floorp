#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
