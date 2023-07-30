import pytest

URL = "https://ib.absa.co.za/absa-online/login.jsp"
UNSUPPORTED_ALERT = "Browser unsupported"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.find_css("html.gecko")


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    alert = await client.await_alert(UNSUPPORTED_ALERT)
    await client.navigate(URL)
    await alert
    assert client.find_css("html.unknown")
