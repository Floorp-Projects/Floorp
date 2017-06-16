# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozhttpd
import urllib2
import os
import unittest

import mozunit

here = os.path.dirname(os.path.abspath(__file__))


class RequestLogTest(unittest.TestCase):

    def check_logging(self, log_requests=False):

        httpd = mozhttpd.MozHttpd(port=0, docroot=here, log_requests=log_requests)
        httpd.start(block=False)
        url = "http://%s:%s/" % ('127.0.0.1', httpd.httpd.server_port)
        f = urllib2.urlopen(url)
        f.read()

        return httpd.request_log

    def test_logging_enabled(self):
        request_log = self.check_logging(log_requests=True)

        self.assertEqual(len(request_log), 1)

        log_entry = request_log[0]
        self.assertEqual(log_entry['method'], 'GET')
        self.assertEqual(log_entry['path'], '/')
        self.assertEqual(type(log_entry['time']), float)

    def test_logging_disabled(self):
        request_log = self.check_logging(log_requests=False)

        self.assertEqual(len(request_log), 0)


if __name__ == '__main__':
    mozunit.main()
