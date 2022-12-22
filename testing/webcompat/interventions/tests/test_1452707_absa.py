import pytest

URL = "https://ib.absa.co.za/absa-online/login.jsp"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.find_css("html.gecko")


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.alert.text == "Browser unsupported"
    client.alert.dismiss()
    assert client.find_css("html.unknown")
