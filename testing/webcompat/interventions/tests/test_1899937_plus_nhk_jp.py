import pytest

URL = "https://plus.nhk.jp/"
UNSUPPORTED_CSS = ".firefox_not_supported"
DENIED_TEXT = "This webpage is not available in your area"


async def check_site(client, should_pass):
    await client.navigate(URL, wait="load")

    denied, unsupported = client.await_first_element_of(
        [
            client.text(DENIED_TEXT),
            client.css(UNSUPPORTED_CSS),
        ]
    )

    if denied:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to Japan.")
        return

    assert (should_pass and not client.is_displayed(unsupported)) or (
        not should_pass and client.is_displayed(unsupported)
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await check_site(client, should_pass=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await check_site(client, should_pass=False)
