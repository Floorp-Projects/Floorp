import pytest

URL = "https://www.bankofamerica.com/"


@pytest.mark.asyncio
@pytest.mark.skip_platforms("android", "windows")
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not client.find_css("#browserUpgradeNoticeBar")


@pytest.mark.asyncio
@pytest.mark.skip_platforms("android", "windows")
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.is_displayed(client.await_css("#browserUpgradeNoticeBar"))
