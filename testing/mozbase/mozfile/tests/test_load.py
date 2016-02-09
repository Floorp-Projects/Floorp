#!/usr/bin/env python

"""
tests for mozfile.load
"""

from wptserve import server, handlers
import os
import tempfile
import unittest
from mozfile import load


class TestLoad(unittest.TestCase):
    """test the load function"""

    def test_http(self):
        """test with wptserve and a http:// URL"""

        @handlers.handler
        def example(request, reponse):
            """example request handler"""
            body = 'example'
            return (200, [{'Content-type': 'text/plain',
                           'Content-length': len(body)
                          }], [body])

        host = '127.0.0.1'
        httpd = server.WebTestHttpd(
            host=host,
            routes=[('GET', "*", example)])
        try:
            httpd.start(block=False)
            content = load(httpd.get_url()).read()
            self.assertEqual(content, 'example')
        finally:
            httpd.stop()

    def test_file_path(self):
        """test loading from file path"""
        try:
            # create a temporary file
            tmp = tempfile.NamedTemporaryFile(delete=False)
            tmp.write('foo bar')
            tmp.close()

            # read the file
            contents = file(tmp.name).read()
            self.assertEqual(contents, 'foo bar')

            # read the file with load and a file path
            self.assertEqual(load(tmp.name).read(), contents)

            # read the file with load and a file URL
            self.assertEqual(load('file://%s' % tmp.name).read(), contents)
        finally:
            # remove the tempfile
            if os.path.exists(tmp.name):
                os.remove(tmp.name)

if __name__ == '__main__':
    unittest.main()
