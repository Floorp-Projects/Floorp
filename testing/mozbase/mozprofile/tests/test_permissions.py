#!/usr/bin/env python

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import mozunit
import pytest
from mozprofile.permissions import Permissions

LOCATIONS = """http://mochi.test:8888  primary,privileged
http://127.0.0.1:8888           privileged
"""


@pytest.fixture
def locations_file(tmpdir):
    locations_file = tmpdir.join("locations.txt")
    locations_file.write(LOCATIONS)
    return locations_file.strpath


@pytest.fixture
def perms(tmpdir, locations_file):
    return Permissions(locations_file)


def test_nw_prefs(perms):
    prefs, user_prefs = perms.network_prefs(False)

    assert len(user_prefs) == 0
    assert len(prefs) == 0

    prefs, user_prefs = perms.network_prefs({"http": 8888})
    assert len(user_prefs) == 2
    assert user_prefs[0] == ("network.proxy.type", 2)
    assert user_prefs[1][0] == "network.proxy.autoconfig_url"

    origins_decl = (
        "var knownOrigins = (function () {  return ['http://mochi.test:8888', "
        "'http://127.0.0.1:8888'].reduce"
    )
    assert origins_decl in user_prefs[1][1]

    proxy_check = (
        "'http': 'PROXY mochi.test:8888'",
        "'https': 'PROXY mochi.test:4443'",
        "'ws': 'PROXY mochi.test:4443'",
        "'wss': 'PROXY mochi.test:4443'",
    )
    assert all(c in user_prefs[1][1] for c in proxy_check)

    prefs, user_prefs = perms.network_prefs({"dohServerPort": 443})
    print(user_prefs)
    assert len(user_prefs) == 5
    assert user_prefs[0] == ("network.proxy.type", 0)
    assert user_prefs[1] == ("network.trr.mode", 3)
    assert user_prefs[2] == ("network.trr.uri", "https://foo.example.com:443/dns-query")
    assert user_prefs[3] == ("network.trr.bootstrapAddr", "127.0.0.1")
    assert user_prefs[4] == ("network.dns.force_use_https_rr", True)


if __name__ == "__main__":
    mozunit.main()
