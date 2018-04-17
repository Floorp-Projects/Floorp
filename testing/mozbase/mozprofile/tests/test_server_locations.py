#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import mozunit
import pytest

from mozprofile.permissions import ServerLocations, \
    MissingPrimaryLocationError, MultiplePrimaryLocationsError, \
    BadPortLocationError, LocationsSyntaxError


LOCATIONS = """# This is the primary location from which tests run.
#
http://mochi.test:8888          primary,privileged

# a few test locations
http://127.0.0.1:80             privileged
http://127.0.0.1:8888           privileged
https://test:80                 privileged
http://example.org:80           privileged
http://test1.example.org        privileged

"""

LOCATIONS_NO_PRIMARY = """http://secondary.test:80        privileged
http://tertiary.test:8888       privileged
"""

LOCATIONS_BAD_PORT = """http://mochi.test:8888  primary,privileged
http://127.0.0.1:80             privileged
http://127.0.0.1:8888           privileged
http://test:badport             privileged
http://example.org:80           privileged
"""


def compare_location(location, scheme, host, port, options):
    assert location.scheme == scheme
    assert location.host == host
    assert location.port == port
    assert location.options == options


@pytest.fixture
def create_temp_file(tmpdir):
    def inner(contents):
        f = tmpdir.mkdtemp().join('locations.txt')
        f.write(contents)
        return f.strpath
    return inner


def test_server_locations(create_temp_file):
    # write a permissions file
    f = create_temp_file(LOCATIONS)

    # read the locations
    locations = ServerLocations(f)

    # ensure that they're what we expect
    assert len(locations) == 6
    i = iter(locations)
    compare_location(next(i), 'http', 'mochi.test', '8888', ['primary', 'privileged'])
    compare_location(next(i), 'http', '127.0.0.1', '80', ['privileged'])
    compare_location(next(i), 'http', '127.0.0.1', '8888', ['privileged'])
    compare_location(next(i), 'https', 'test', '80', ['privileged'])
    compare_location(next(i), 'http', 'example.org', '80', ['privileged'])
    compare_location(next(i), 'http', 'test1.example.org', '8888', ['privileged'])

    locations.add_host('mozilla.org')
    assert len(locations) == 7
    compare_location(next(i), 'http', 'mozilla.org', '80', ['privileged'])

    # test some errors
    with pytest.raises(MultiplePrimaryLocationsError):
        locations.add_host('primary.test', options='primary')

    # assert we don't throw DuplicateLocationError
    locations.add_host('127.0.0.1')

    with pytest.raises(BadPortLocationError):
        locations.add_host('127.0.0.1', port='abc')

    # test some errors in locations file
    f = create_temp_file(LOCATIONS_NO_PRIMARY)

    exc = None
    try:
        ServerLocations(f)
    except LocationsSyntaxError as e:
        exc = e
    assert exc is not None
    assert exc.err.__class__ == MissingPrimaryLocationError
    assert exc.lineno == 3

    # test bad port in a locations file to ensure lineno calculated
    # properly.
    f = create_temp_file(LOCATIONS_BAD_PORT)

    exc = None
    try:
        ServerLocations(f)
    except LocationsSyntaxError as e:
        exc = e
    assert exc is not None
    assert exc.err.__class__ == BadPortLocationError
    assert exc.lineno == 4


def test_server_locations_callback(create_temp_file):
    class CallbackTest(object):
        last_locations = None

        def callback(self, locations):
            self.last_locations = locations

    c = CallbackTest()
    f = create_temp_file(LOCATIONS)
    locations = ServerLocations(f, c.callback)

    # callback should be for all locations in file
    assert len(c.last_locations) == 6

    # validate arbitrary one
    compare_location(c.last_locations[2], 'http', '127.0.0.1', '8888', ['privileged'])

    locations.add_host('a.b.c')

    # callback should be just for one location
    assert len(c.last_locations) == 1
    compare_location(c.last_locations[0], 'http', 'a.b.c', '80', ['privileged'])

    # read a second file, which should generate a callback with both
    # locations.
    f = create_temp_file(LOCATIONS_NO_PRIMARY)
    locations.read(f)
    assert len(c.last_locations) == 2


if __name__ == '__main__':
    mozunit.main()
