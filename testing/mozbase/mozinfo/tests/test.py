#!/usr/bin/env python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
import sys
from importlib import reload
from unittest import mock

import mozinfo
import mozunit
import pytest


@pytest.fixture(autouse=True)
def on_every_test():
    # per-test set up
    reload(mozinfo)

    # When running from an objdir mozinfo will use a build generated json file
    # instead of the ones created for testing. Prevent that from happening.
    # See bug 896038 for details.
    sys.modules["mozbuild"] = None

    yield

    # per-test tear down
    del sys.modules["mozbuild"]


def test_basic():
    """Test that mozinfo has a few attributes."""
    assert mozinfo.os is not None
    # should have isFoo == True where os == "foo"
    assert getattr(mozinfo, "is" + mozinfo.os[0].upper() + mozinfo.os[1:])


def test_update():
    """Test that mozinfo.update works."""
    mozinfo.update({"foo": 123})
    assert mozinfo.info["foo"] == 123


def test_update_file(tmpdir):
    """Test that mozinfo.update can load a JSON file."""
    j = os.path.join(tmpdir, "mozinfo.json")
    with open(j, "w") as f:
        f.write(json.dumps({"foo": "xyz"}))
    mozinfo.update(j)
    assert mozinfo.info["foo"] == "xyz"


def test_update_file_invalid_json(tmpdir):
    """Test that mozinfo.update handles invalid JSON correctly"""
    j = os.path.join(tmpdir, "test.json")
    with open(j, "w") as f:
        f.write('invalid{"json":')
    with pytest.raises(ValueError):
        mozinfo.update([j])


def test_find_and_update_file(tmpdir):
    """Test that mozinfo.find_and_update_from_json can
    find mozinfo.json in a directory passed to it."""
    j = os.path.join(tmpdir, "mozinfo.json")
    with open(j, "w") as f:
        f.write(json.dumps({"foo": "abcdefg"}))
    assert mozinfo.find_and_update_from_json(tmpdir) == j
    assert mozinfo.info["foo"] == "abcdefg"


def test_find_and_update_file_no_argument():
    """Test that mozinfo.find_and_update_from_json no-ops on not being
    given any arguments.
    """
    assert mozinfo.find_and_update_from_json() is None


def test_find_and_update_file_invalid_json(tmpdir):
    """Test that mozinfo.find_and_update_from_json can
    handle invalid JSON"""
    j = os.path.join(tmpdir, "mozinfo.json")
    with open(j, "w") as f:
        f.write('invalid{"json":')
    with pytest.raises(ValueError):
        mozinfo.find_and_update_from_json(tmpdir)


def test_find_and_update_file_raise_exception():
    """Test that mozinfo.find_and_update_from_json raises
    an IOError when exceptions are unsuppressed.
    """
    with pytest.raises(IOError):
        mozinfo.find_and_update_from_json(raise_exception=True)


def test_find_and_update_file_suppress_exception():
    """Test that mozinfo.find_and_update_from_json suppresses
    an IOError exception if a False boolean value is
    provided as the only argument.
    """
    assert mozinfo.find_and_update_from_json(raise_exception=False) is None


def test_find_and_update_file_mozbuild(tmpdir):
    """Test that mozinfo.find_and_update_from_json can
    find mozinfo.json using the mozbuild module."""
    j = os.path.join(tmpdir, "mozinfo.json")
    with open(j, "w") as f:
        f.write(json.dumps({"foo": "123456"}))
    m = mock.MagicMock()
    # Mock the value of MozbuildObject.from_environment().topobjdir.
    m.MozbuildObject.from_environment.return_value.topobjdir = tmpdir

    mocked_modules = {
        "mozbuild": m,
        "mozbuild.base": m,
        "mozbuild.mozconfig": m,
    }
    with mock.patch.dict(sys.modules, mocked_modules):
        assert mozinfo.find_and_update_from_json() == j
    assert mozinfo.info["foo"] == "123456"


def test_output_to_file(tmpdir):
    """Test that mozinfo.output_to_file works."""
    path = os.path.join(tmpdir, "mozinfo.json")
    mozinfo.output_to_file(path)
    assert open(path).read() == json.dumps(mozinfo.info)


def test_os_version_is_a_StringVersion():
    assert isinstance(mozinfo.os_version, mozinfo.StringVersion)


def test_compare_to_string():
    version = mozinfo.StringVersion("10.10")

    assert version > "10.2"
    assert "11" > version
    assert version >= "10.10"
    assert "10.11" >= version
    assert version == "10.10"
    assert "10.10" == version
    assert version != "10.2"
    assert "11" != version
    assert version < "11.8.5"
    assert "10.2" < version
    assert version <= "11"
    assert "10.10" <= version

    # Can have non-numeric versions (Bug 1654915)
    assert version != mozinfo.StringVersion("Testing")
    assert mozinfo.StringVersion("Testing") != version
    assert mozinfo.StringVersion("") == ""
    assert "" == mozinfo.StringVersion("")

    a = mozinfo.StringVersion("1.2.5a")
    b = mozinfo.StringVersion("1.2.5b")
    assert a < b
    assert b > a

    # Make sure we can compare against unicode (for python 2).
    assert a == "1.2.5a"
    assert "1.2.5a" == a


def test_to_string():
    assert "10.10" == str(mozinfo.StringVersion("10.10"))


if __name__ == "__main__":
    mozunit.main()
