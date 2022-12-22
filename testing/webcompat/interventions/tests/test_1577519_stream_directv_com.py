import pytest

URL = "https://stream.directv.com/"
LOGIN_CSS = "#userID"
UNSUPPORTED_CSS = ".title-new-browse-ff"
DENIED_XPATH = "//h1[text()='Access Denied']"


async def check_site(client, should_pass):
    await client.navigate(URL)

    denied, login, unsupported = client.await_first_element_of(
        [
            client.xpath(DENIED_XPATH),
            client.css(LOGIN_CSS),
            client.css(UNSUPPORTED_CSS),
        ],
        is_displayed=True,
    )

    if denied:
        pytest.skip("Region-locked, cannot test. Try using a VPN set to USA.")
        return

    assert (should_pass and login) or (not should_pass and unsupported)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await check_site(client, should_pass=True)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await check_site(client, should_pass=False)
