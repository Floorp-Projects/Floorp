# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

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
