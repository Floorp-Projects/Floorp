import pytest

URL = (
    "https://watch.antennaplus.gr/#/shows/agries_melisses/seasons/"
    "3/episode/agries_melisses_S03_E137_v1"
)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.await_css(".login-pf-page")


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(".ua-barrier")
