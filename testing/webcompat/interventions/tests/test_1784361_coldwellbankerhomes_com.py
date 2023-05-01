import pytest

# The gallery on this page only sets img[src] to a non-1x1-spacer if the error
# doesn't happen during page load, which doesn't happen with a Chrome UA.

URL = "https://www.coldwellbankerhomes.com/ri/little-compton/kvc-17_1,17_2/"
ERROR_MSG = 'can\'t access property "dataset", v[0] is undefined'
SUCCESS_CSS = "img.psr-lazy:not([src*='spacer'])"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SUCCESS_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
