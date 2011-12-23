#!/usr/bin/env python

import os
import shutil
import tempfile
import unittest
from mozprofile.permissions import PermissionsManager

class ServerLocationsTest(unittest.TestCase):
    """test server locations"""

    locations = """
# This is the primary location from which tests run.
#
http://mochi.test:8888   primary,privileged
    
# a few test locations
http://127.0.0.1:80               privileged
http://127.0.0.1:8888             privileged
https://test:80                    privileged
http://mochi.test:8888            privileged
http://example.org:80                privileged
http://test1.example.org:80          privileged

    """

    def compare_location(self, location, scheme, host, port, options):
        self.assertEqual(location.scheme, scheme)
        self.assertEqual(location.host, host)
        self.assertEqual(location.port, port)
        self.assertEqual(location.options, options)

    def test_server_locations(self):

        # make a permissions manager
        # needs a pointless temporary directory for now
        tempdir = tempfile.mkdtemp()
        permissions = PermissionsManager(tempdir)

        # write a permissions file
        fd, filename = tempfile.mkstemp()
        os.write(fd, self.locations)
        os.close(fd)

        # read the locations
        locations = permissions.read_locations(filename)

        # ensure that they're what we expect
        self.assertEqual(len(locations), 7)
        self.compare_location(locations[0], 'http', 'mochi.test', '8888', ['primary', 'privileged'])
        self.compare_location(locations[1], 'http', '127.0.0.1', '80', ['privileged'])
        self.compare_location(locations[2], 'http', '127.0.0.1', '8888', ['privileged'])
        self.compare_location(locations[3], 'https', 'test', '80', ['privileged'])
        self.compare_location(locations[4], 'http', 'mochi.test', '8888', ['privileged'])
        self.compare_location(locations[5], 'http', 'example.org', '80', ['privileged'])
        self.compare_location(locations[6], 'http', 'test1.example.org', '80', ['privileged'])

        # cleanup
        del permissions
        shutil.rmtree(tempdir)
        os.remove(filename)


if __name__ == '__main__':
    unittest.main()
