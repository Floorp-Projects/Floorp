import pytest

URL = "https://conference.amwell.com/"
UNSUPPORTED_DESKTOP_CSS = "a.container-recommended-browser"
UNSUPPORTED_MOBILE_CSS = "[onclick='joinWithApp()']"
LOGIN_CSS = "login-layout"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(LOGIN_CSS, is_displayed=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    desktop, mobile = client.await_first_element_of(
        [client.css(UNSUPPORTED_DESKTOP_CSS), client.css(UNSUPPORTED_MOBILE_CSS)],
        is_displayed=True,
    )
    assert desktop or mobile
