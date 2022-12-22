import pytest

URL = "https://buskocchi.desuca.co.jp/smartPhone.html"
SCRIPT = "return document.getElementById('map_canvas')?.clientHeight"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.execute_script(SCRIPT)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.execute_script(SCRIPT)
