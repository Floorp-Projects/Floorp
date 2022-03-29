# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

import pytest


@pytest.fixture
def base_schema():
    def inner():
        return {
            "namespace": "testAPIName",
            "permissions": [],
            "types": [],
            "functions": [],
            "events": [],
        }

    return inner


@pytest.fixture
def write_jsonschema_fixtures(tmpdir):
    """Write test schema data into per-testcase (in tmpdir or the given directory)"""

    def inner(jsonschema_fixtures, targetdir=None):
        assert jsonschema_fixtures
        if targetdir is None:
            targetdir = tmpdir
        for filename, filecontent in jsonschema_fixtures.items():
            assert isinstance(filename, str) and filename
            assert isinstance(filecontent, str) and filecontent
            with open(os.path.join(targetdir, filename), "w") as jsonfile:
                jsonfile.write(filecontent)
        return targetdir

    return inner
