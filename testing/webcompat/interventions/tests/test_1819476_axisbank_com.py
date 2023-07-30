import pytest

URL = "https://www.axisbank.com/retail/cards/credit-card"
SITE_CSS = "#ulCreditCard:not(:empty)"
ERROR_MSG = "webkitSpeechRecognition is not defined"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(SITE_CSS, timeout=120)  # the site can be slow


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG, timeout=10)
