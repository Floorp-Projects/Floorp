import mozhttpd
import unittest

import mozunit


class BaseUrlTest(unittest.TestCase):

    def test_base_url(self):
        httpd = mozhttpd.MozHttpd(port=0)
        self.assertEqual(httpd.get_url(), None)
        httpd.start(block=False)
        self.assertEqual("http://127.0.0.1:%s/" % httpd.httpd.server_port,
                         httpd.get_url())
        self.assertEqual("http://127.0.0.1:%s/cheezburgers.html" %
                         httpd.httpd.server_port,
                         httpd.get_url(path="/cheezburgers.html"))
        httpd.stop()


if __name__ == '__main__':
    mozunit.main()
