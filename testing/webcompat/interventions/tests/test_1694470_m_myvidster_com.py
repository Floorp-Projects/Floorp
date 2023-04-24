import pytest

URL = "https://m.myvidster.com/"
CONTAINER_CSS = "#home_refresh_var"


def content_is_cropped(client):
    container = client.await_css(CONTAINER_CSS, is_displayed=True)
    assert container
    return client.execute_script(
        """
        return arguments[0].clientHeight < 100
    """,
        container,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert not content_is_cropped(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert content_is_cropped(client)
