import pytest

URL = "https://www.slushy.com/"


# skip Android, as the site blocks low-res devices like the emulator
@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL, wait="load")
    assert not client.execute_script("return location.href.includes('unsupported')")


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, wait="load")
    assert client.execute_script("return location.href.includes('unsupported')")
