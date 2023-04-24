import pytest

URL = "https://www.teletrader.com/"


def body_is_offscreen(client):
    return client.execute_script(
        """
        return document.body.getBoundingClientRect().left > 0;
    """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not body_is_offscreen(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert body_is_offscreen(client)
