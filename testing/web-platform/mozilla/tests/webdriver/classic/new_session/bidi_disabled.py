from copy import deepcopy


def test_marionette_fallback_webdriver_session(configuration, geckodriver):
    config = deepcopy(configuration)
    config["capabilities"]["webSocketUrl"] = True

    prefs = config["capabilities"]["moz:firefoxOptions"].get("prefs", {})
    prefs.update({"remote.active-protocols": 2})
    config["capabilities"]["moz:firefoxOptions"]["prefs"] = prefs

    try:
        driver = geckodriver(config=config)
        driver.new_session()

        assert driver.session.capabilities.get("webSocketUrl") is None

        # Sanity check that Marionette works as expected and by default returns
        # at least one window handle
        assert len(driver.session.handles) >= 1

    finally:
        driver.stop()

        # WebDriver BiDi has to be re-enabled. Because we cannot easily
        # get rid of the value let geckodriver overwrite it with the current
        # default.
        prefs.update({"remote.active-protocols": 3})

        driver = geckodriver(config=config)
        driver.new_session()

        assert driver.session.capabilities.get("webSocketUrl") is not None
