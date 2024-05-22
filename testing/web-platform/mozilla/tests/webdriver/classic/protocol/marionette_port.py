import os
from copy import deepcopy

import pytest

pytestmark = pytest.mark.asyncio


@pytest.mark.parametrize("port", ["0", "2828"], ids=["system allocated", "fixed"])
async def test_marionette_port(geckodriver, port):
    extra_args = ["--marionette-port", port]

    driver = geckodriver(extra_args=extra_args)
    driver.new_session()
    await driver.delete_session()


async def test_marionette_port_outdated_active_port_file(
    configuration, create_custom_profile, geckodriver
):
    config = deepcopy(configuration)
    custom_profile = create_custom_profile()
    extra_args = ["--marionette-port", "0"]

    # Prepare a Marionette active port file that contains a port which will
    # never be used when requesting a system allocated port.
    active_port_file = os.path.join(custom_profile.profile, "MarionetteActivePort")
    with open(active_port_file, "wb") as f:
        f.write(b"53")

    config["capabilities"]["moz:firefoxOptions"]["args"] = [
        "--profile",
        custom_profile.profile,
    ]

    driver = geckodriver(config=config, extra_args=extra_args)

    driver.new_session()
    with open(active_port_file, "rb") as f:
        assert f.readline() != b"53"

    await driver.delete_session()
    with pytest.raises(FileNotFoundError):
        open(active_port_file, "rb")
