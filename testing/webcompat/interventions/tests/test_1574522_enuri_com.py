import pytest

# The site puts an exclamation mark in front of width="703"
# for Chrome UAs, but not for Firefox.

URL = "http://enuri.com/knowcom/detail.jsp?kbno=474985"
CHECK_CSS = "td[width='703']"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not client.find_css(CHECK_CSS)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.find_css(CHECK_CSS)
