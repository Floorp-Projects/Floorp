#!/usr/bin/env python

import mozhttpd
import mozfile
import os
import tempfile
import unittest

import mozunit


class TestBasic(unittest.TestCase):
    """ Test basic Mozhttpd capabilites """

    def test_basic(self):
        """ Test mozhttpd can serve files """

        tempdir = tempfile.mkdtemp()

        # sizes is a dict of the form: name -> [size, binary_string, filepath]
        sizes = {'small': [128], 'large': [16384]}

        for k in sizes.keys():
            # Generate random binary string
            sizes[k].append(os.urandom(sizes[k][0]))

            # Add path of file with binary string to list
            fpath = os.path.join(tempdir, k)
            sizes[k].append(fpath)

            # Write binary string to file
            with open(fpath, 'wb') as f:
                f.write(sizes[k][1])

        server = mozhttpd.MozHttpd(docroot=tempdir)
        server.start()
        server_url = server.get_url()

        # Retrieve file and check contents matchup
        for k in sizes.keys():
            retrieved_content = mozfile.load(server_url + k).read()
            self.assertEqual(retrieved_content, sizes[k][1])

        # Cleanup tempdir and related files
        mozfile.rmtree(tempdir)


if __name__ == '__main__':
    mozunit.main()
