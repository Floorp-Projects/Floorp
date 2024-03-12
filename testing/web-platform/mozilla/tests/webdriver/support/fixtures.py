import pytest
from mozprofile import Profile

from .helpers import (
    Browser,
    Geckodriver,
    get_pref,
    get_profile_folder,
    read_user_preferences,
    set_pref,
)


@pytest.fixture(scope="module")
def browser(configuration):
    """Start a Firefox instance without using geckodriver.

    geckodriver will automatically use the --remote-allow-hosts and
    --remote.allow.origins command line arguments.

    Starting Firefox without geckodriver allows to set those command line arguments
    as needed. The fixture method returns the browser instance that should be used
    to connect to a RemoteAgent supported protocol (CDP, WebDriver BiDi).
    """
    current_browser = None

    def _browser(use_bidi=False, use_cdp=False, extra_args=None, extra_prefs=None):
        nonlocal current_browser

        # If the requested preferences and arguments match the ones for the
        # already started firefox, we can reuse the current firefox instance,
        # return the instance immediately.
        if current_browser:
            if (
                current_browser.use_bidi == use_bidi
                and current_browser.use_cdp == use_cdp
                and current_browser.extra_args == extra_args
                and current_browser.extra_prefs == extra_prefs
                and current_browser.is_running
            ):
                return current_browser

            # Otherwise, if firefox is already started, terminate it because we need
            # to create a new instance for the provided preferences.
            current_browser.quit()

        binary = configuration["browser"]["binary"]
        env = configuration["browser"]["env"]
        firefox_options = configuration["capabilities"]["moz:firefoxOptions"]
        current_browser = Browser(
            binary,
            firefox_options,
            use_bidi=use_bidi,
            use_cdp=use_cdp,
            extra_args=extra_args,
            extra_prefs=extra_prefs,
            env=env,
        )
        current_browser.start()
        return current_browser

    yield _browser

    # Stop firefox at the end of the test module.
    if current_browser is not None:
        current_browser.quit()
        current_browser = None


@pytest.fixture
def custom_profile(profile_folder):
    # Clone the known profile for automation preferences
    profile = Profile.clone(profile_folder)

    yield profile

    profile.cleanup()


@pytest.fixture
def default_preferences(profile_folder):
    return read_user_preferences(profile_folder)


@pytest.fixture
def firefox_options(configuration):
    return configuration["capabilities"]["moz:firefoxOptions"]


@pytest.fixture
def geckodriver(configuration):
    """Start a geckodriver instance directly."""
    driver = None

    def _geckodriver(config=None, hostname=None, extra_args=None):
        nonlocal driver

        if config is None:
            config = configuration

        driver = Geckodriver(config, hostname, extra_args)
        driver.start()

        return driver

    yield _geckodriver

    if driver is not None:
        driver.stop()


@pytest.fixture
def profile_folder(firefox_options):
    return get_profile_folder(firefox_options)


@pytest.fixture
def use_pref(session):
    """Set a specific pref value."""
    reset_values = {}

    def _use_pref(pref, value):
        if pref not in reset_values:
            reset_values[pref] = get_pref(session, pref)

        set_pref(session, pref, value)

    yield _use_pref

    for pref, reset_value in reset_values.items():
        set_pref(session, pref, reset_value)
