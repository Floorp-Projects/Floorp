#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozfile
import unittest
from mozprofile.permissions import ServerLocations, \
    MissingPrimaryLocationError, MultiplePrimaryLocationsError, \
    DuplicateLocationError, BadPortLocationError, LocationsSyntaxError

class ServerLocationsTest(unittest.TestCase):
    """test server locations"""

    locations = """# This is the primary location from which tests run.
#
http://mochi.test:8888          primary,privileged

# a few test locations
http://127.0.0.1:80             privileged
http://127.0.0.1:8888           privileged
https://test:80                 privileged
http://example.org:80           privileged
http://test1.example.org        privileged

    """

    locations_no_primary = """http://secondary.test:80        privileged
http://tertiary.test:8888       privileged
"""

    locations_bad_port = """http://mochi.test:8888  primary,privileged
http://127.0.0.1:80             privileged
http://127.0.0.1:8888           privileged
http://test:badport             privileged
http://example.org:80           privileged
"""

    def compare_location(self, location, scheme, host, port, options):
        self.assertEqual(location.scheme, scheme)
        self.assertEqual(location.host, host)
        self.assertEqual(location.port, port)
        self.assertEqual(location.options, options)

    def create_temp_file(self, contents):
        f = mozfile.NamedTemporaryFile()
        f.write(contents)
        f.flush()
        return f

    def test_server_locations(self):
        # write a permissions file
        f = self.create_temp_file(self.locations)

        # read the locations
        locations = ServerLocations(f.name)

        # ensure that they're what we expect
        self.assertEqual(len(locations), 6)
        i = iter(locations)
        self.compare_location(i.next(), 'http', 'mochi.test', '8888',
                              ['primary', 'privileged'])
        self.compare_location(i.next(), 'http', '127.0.0.1', '80',
                              ['privileged'])
        self.compare_location(i.next(), 'http', '127.0.0.1', '8888',
                              ['privileged'])
        self.compare_location(i.next(), 'https', 'test', '80', ['privileged'])
        self.compare_location(i.next(), 'http', 'example.org', '80',
                              ['privileged'])
        self.compare_location(i.next(), 'http', 'test1.example.org', '8888',
                              ['privileged'])

        locations.add_host('mozilla.org')
        self.assertEqual(len(locations), 7)
        self.compare_location(i.next(), 'http', 'mozilla.org', '80',
                              ['privileged'])

        # test some errors
        self.assertRaises(MultiplePrimaryLocationsError, locations.add_host,
                          'primary.test', options='primary')

        # We no longer throw these DuplicateLocation Error
        try:
            locations.add_host('127.0.0.1')
        except DuplicateLocationError:
            self.assertTrue(False, "Should no longer throw DuplicateLocationError")

        self.assertRaises(BadPortLocationError, locations.add_host, '127.0.0.1',
                          port='abc')

        # test some errors in locations file
        f = self.create_temp_file(self.locations_no_primary)

        exc = None
        try:
            ServerLocations(f.name)
        except LocationsSyntaxError, e:
            exc = e
        self.assertNotEqual(exc, None)
        self.assertEqual(exc.err.__class__, MissingPrimaryLocationError)
        self.assertEqual(exc.lineno, 3)

        # test bad port in a locations file to ensure lineno calculated
        # properly.
        f = self.create_temp_file(self.locations_bad_port)

        exc = None
        try:
            ServerLocations(f.name)
        except LocationsSyntaxError, e:
            exc = e
        self.assertNotEqual(exc, None)
        self.assertEqual(exc.err.__class__, BadPortLocationError)
        self.assertEqual(exc.lineno, 4)

    def test_server_locations_callback(self):
        class CallbackTest(object):
            last_locations = None

            def callback(self, locations):
                self.last_locations = locations

        c = CallbackTest()
        f = self.create_temp_file(self.locations)
        locations = ServerLocations(f.name, c.callback)

        # callback should be for all locations in file
        self.assertEqual(len(c.last_locations), 6)

        # validate arbitrary one
        self.compare_location(c.last_locations[2], 'http', '127.0.0.1', '8888',
                              ['privileged'])

        locations.add_host('a.b.c')

        # callback should be just for one location
        self.assertEqual(len(c.last_locations), 1)
        self.compare_location(c.last_locations[0], 'http', 'a.b.c', '80',
                              ['privileged'])

        # read a second file, which should generate a callback with both
        # locations.
        f = self.create_temp_file(self.locations_no_primary)
        locations.read(f.name)
        self.assertEqual(len(c.last_locations), 2)


if __name__ == '__main__':
    unittest.main()
