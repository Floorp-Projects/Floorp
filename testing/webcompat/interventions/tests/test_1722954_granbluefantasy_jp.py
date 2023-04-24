import pytest

URL = "https://game.granbluefantasy.jp/"


def content_is_full_width(client):
    return client.execute_script(
        """
        return document.querySelector("#wrapper").offsetWidth == document.body.clientWidth;
    """
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert content_is_full_width(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not content_is_full_width(client)
