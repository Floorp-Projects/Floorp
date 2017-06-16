#!/usr/bin/env python

# Any copyright is dedicated to the Public Domain.
# http://creativecommons.org/publicdomain/zero/1.0/

from mozfile import TemporaryDirectory
import mozhttpd
import os
import unittest
import urllib2

import mozunit


class PathTest(unittest.TestCase):

    def try_get(self, url, expected_contents):
        f = urllib2.urlopen(url)
        self.assertEqual(f.getcode(), 200)
        self.assertEqual(f.read(), expected_contents)

    def try_get_expect_404(self, url):
        with self.assertRaises(urllib2.HTTPError) as cm:
            urllib2.urlopen(url)
        self.assertEqual(404, cm.exception.code)

    def test_basic(self):
        """Test that requests to docroot and a path mapping work as expected."""
        with TemporaryDirectory() as d1, TemporaryDirectory() as d2:
            open(os.path.join(d1, "test1.txt"), "w").write("test 1 contents")
            open(os.path.join(d2, "test2.txt"), "w").write("test 2 contents")
            httpd = mozhttpd.MozHttpd(port=0,
                                      docroot=d1,
                                      path_mappings={'/files': d2}
                                      )
            httpd.start(block=False)
            self.try_get(httpd.get_url("/test1.txt"), "test 1 contents")
            self.try_get(httpd.get_url("/files/test2.txt"), "test 2 contents")
            self.try_get_expect_404(httpd.get_url("/files/test2_nope.txt"))
            httpd.stop()

    def test_substring_mappings(self):
        """Test that a path mapping that's a substring of another works."""
        with TemporaryDirectory() as d1, TemporaryDirectory() as d2:
            open(os.path.join(d1, "test1.txt"), "w").write("test 1 contents")
            open(os.path.join(d2, "test2.txt"), "w").write("test 2 contents")
            httpd = mozhttpd.MozHttpd(port=0,
                                      path_mappings={'/abcxyz': d1,
                                                     '/abc': d2, }
                                      )
            httpd.start(block=False)
            self.try_get(httpd.get_url("/abcxyz/test1.txt"), "test 1 contents")
            self.try_get(httpd.get_url("/abc/test2.txt"), "test 2 contents")
            httpd.stop()

    def test_multipart_path_mapping(self):
        """Test that a path mapping with multiple directories works."""
        with TemporaryDirectory() as d1:
            open(os.path.join(d1, "test1.txt"), "w").write("test 1 contents")
            httpd = mozhttpd.MozHttpd(port=0,
                                      path_mappings={'/abc/def/ghi': d1}
                                      )
            httpd.start(block=False)
            self.try_get(httpd.get_url("/abc/def/ghi/test1.txt"), "test 1 contents")
            self.try_get_expect_404(httpd.get_url("/abc/test1.txt"))
            self.try_get_expect_404(httpd.get_url("/abc/def/test1.txt"))
            httpd.stop()

    def test_no_docroot(self):
        """Test that path mappings with no docroot work."""
        with TemporaryDirectory() as d1:
            httpd = mozhttpd.MozHttpd(port=0,
                                      path_mappings={'/foo': d1})
            httpd.start(block=False)
            self.try_get_expect_404(httpd.get_url())
            httpd.stop()


if __name__ == '__main__':
    mozunit.main()
