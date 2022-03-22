from copy import deepcopy
import os

import pytest
from mozprofile import Profile

from . import Geckodriver


@pytest.fixture
def custom_profile(configuration):
    # Clone the known profile for automation preferences
    firefox_options = configuration["capabilities"]["moz:firefoxOptions"]
    _, profile_folder = firefox_options["args"]
    profile = Profile.clone(profile_folder)

    yield profile

    profile.cleanup()


@pytest.mark.parametrize("port", ["0", "2828"], ids=["system allocated", "fixed"])
def test_marionette_port(configuration, custom_profile, port):
    config = deepcopy(configuration)

    # Prepare a Marionette active port file that contains a port which will
    # never be used when requesting a system allocated port.
    active_port_file = os.path.join(custom_profile.profile, "MarionetteActivePort")
    with open(active_port_file, "wb") as f:
        f.write(b"53")

    config["capabilities"]["moz:firefoxOptions"]["args"] = [
        "--profile",
        custom_profile.profile,
    ]

    extra_args = ["--marionette-port", port]
    with Geckodriver(config, "localhost", extra_args=extra_args) as geckodriver:
        geckodriver.new_session()
