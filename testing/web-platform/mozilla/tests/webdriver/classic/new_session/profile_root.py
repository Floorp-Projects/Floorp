import copy
import os

import pytest


def test_profile_root(tmp_path, configuration, geckodriver, user_prefs):
    profile_path = os.path.join(tmp_path, "geckodriver-test")
    os.makedirs(profile_path)

    config = copy.deepcopy(configuration)

    # Pass all the wpt preferences from the default profile's user.js via
    # capabilities to allow geckodriver to create a new valid profile itself.
    config["capabilities"]["moz:firefoxOptions"]["prefs"] = user_prefs

    # Ensure we don't set a profile in command line arguments
    del config["capabilities"]["moz:firefoxOptions"]["args"]

    extra_args = ["--profile-root", profile_path]

    assert os.listdir(profile_path) == []

    driver = geckodriver(config=config, extra_args=extra_args)
    driver.new_session()
    assert len(os.listdir(profile_path)) == 1
    driver.delete_session()
    assert os.listdir(profile_path) == []


def test_profile_root_missing(tmp_path, configuration, geckodriver):
    profile_path = os.path.join(tmp_path, "missing-path")
    assert not os.path.exists(profile_path)

    config = copy.deepcopy(configuration)
    # Ensure we don't set a profile in command line arguments
    del config["capabilities"]["moz:firefoxOptions"]["args"]

    extra_args = ["--profile-root", profile_path]

    with pytest.raises(ChildProcessError) as exc_info:
        geckodriver(config=config, extra_args=extra_args)
    assert str(exc_info.value) == "geckodriver terminated with code 64"
