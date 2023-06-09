import pytest

URL = "https://www.thai-masszazs.net/"


async def is_scrolling_broken(client):
    await client.navigate(URL)
    return client.execute_script(
        """
            return document.querySelector("html").style.overflow == "hidden"
        """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_scrolling_broken(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_scrolling_broken(client)
