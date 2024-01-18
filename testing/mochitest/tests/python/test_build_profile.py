# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import json
import os
from argparse import Namespace

import mozunit
import pytest
from conftest import setup_args
from mozbuild.base import MozbuildObject
from mozprofile import Profile
from mozprofile.prefs import Preferences
from six import string_types

here = os.path.abspath(os.path.dirname(__file__))


@pytest.fixture
def build_profile(monkeypatch, setup_test_harness, parser):
    setup_test_harness(*setup_args)
    runtests = pytest.importorskip("runtests")
    md = runtests.MochitestDesktop("plain", {"log_tbpl": "-"})
    monkeypatch.setattr(md, "fillCertificateDB", lambda *args, **kwargs: None)

    options = parser.parse_args([])
    options = vars(options)

    def inner(**kwargs):
        opts = options.copy()
        opts.update(kwargs)

        return md, md.buildProfile(Namespace(**opts))

    return inner


@pytest.fixture
def profile_data_dir():
    build = MozbuildObject.from_environment(cwd=here)
    return os.path.join(build.topsrcdir, "testing", "profiles")


def test_common_prefs_are_all_set(build_profile, profile_data_dir):
    md, result = build_profile()

    with open(os.path.join(profile_data_dir, "profiles.json"), "r") as fh:
        base_profiles = json.load(fh)["mochitest"]

    # build the expected prefs
    expected_prefs = {}
    for profile in base_profiles:
        for name in Profile.preference_file_names:
            path = os.path.join(profile_data_dir, profile, name)
            if os.path.isfile(path):
                expected_prefs.update(Preferences.read_prefs(path))

    # read the actual prefs
    actual_prefs = {}
    for name in Profile.preference_file_names:
        path = os.path.join(md.profile.profile, name)
        if os.path.isfile(path):
            actual_prefs.update(Preferences.read_prefs(path))

    # keep this in sync with the values in MochitestDesktop.merge_base_profiles
    interpolation = {
        "server": "127.0.0.1:8888",
    }
    for k, v in expected_prefs.items():
        if isinstance(v, string_types):
            v = v.format(**interpolation)

        assert k in actual_prefs
        assert k and actual_prefs[k] == v


if __name__ == "__main__":
    mozunit.main()
