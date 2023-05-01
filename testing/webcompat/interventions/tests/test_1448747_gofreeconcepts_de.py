import pytest

URL = "https://gofreeconcepts.de/collections/shamma-sandals/products/shamma-sandals-warriors-maximus-mit-lederfussbett"
SELECT_CSS = "#productSelect"


async def is_fastclick_active(client):
    await client.navigate(URL)
    return client.test_for_fastclick(client.await_css(SELECT_CSS))


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_fastclick_active(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_fastclick_active(client)
