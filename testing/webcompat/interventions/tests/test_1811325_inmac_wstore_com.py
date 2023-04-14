import pytest

URL = "https://www.inmac-wstore.com"
SITE_CSS = ".desktopDevice"
IFRAME_CSS = "iframe[src^='https://geo.captcha-delivery.com/']"
BLOCKED_CSS = ".captcha__human"
NOT_A_ROBOT_TEXT = (
    "We want to make sure it is actually you we are dealing with and not a robot."
)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)

    frame = client.find_css(IFRAME_CSS)
    if frame:
        client.switch_frame(frame)

    blocked, loaded = client.await_first_element_of(
        [
            client.text(NOT_A_ROBOT_TEXT),
            client.css(SITE_CSS),
        ]
    )

    if blocked:
        pytest.skip("Site using a captcha, can't proceed with test.")
        return

    assert client.is_displayed(loaded)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    frame = client.find_css(IFRAME_CSS)
    client.switch_frame(frame)
    assert client.await_css(BLOCKED_CSS)
