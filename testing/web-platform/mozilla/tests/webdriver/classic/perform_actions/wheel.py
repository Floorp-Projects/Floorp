from copy import deepcopy

import pytest
from tests.classic.perform_actions.support.refine import get_events


@pytest.mark.parametrize("device_pixel_ratio", ["1.0", "2.0", "0.5"])
def test_scroll_delta_device_pixel(configuration, url, geckodriver, device_pixel_ratio):
    config = deepcopy(configuration)

    prefs = config["capabilities"]["moz:firefoxOptions"].get("prefs", {})
    prefs.update({"layout.css.devPixelsPerPx": device_pixel_ratio})
    config["capabilities"]["moz:firefoxOptions"]["prefs"] = prefs

    try:
        driver = geckodriver(config=config)
        driver.new_session()

        driver.session.url = url(
            "/webdriver/tests/support/html/test_actions_scroll.html"
        )

        target = driver.session.find.css("#scrollable", all=False)

        chain = driver.session.actions.sequence("wheel", "wheel_id")
        chain.scroll(0, 0, 5, 10, origin=target).perform()

        events = get_events(driver.session)
        assert len(events) == 1
        assert events[0]["type"] == "wheel"
        assert events[0]["deltaX"] == 5
        assert events[0]["deltaY"] == 10
        assert events[0]["deltaZ"] == 0
        assert events[0]["target"] == "scrollable-content"

    finally:
        driver.stop()
