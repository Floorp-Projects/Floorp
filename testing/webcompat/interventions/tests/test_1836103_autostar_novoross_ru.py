import pytest

URL = "https://autostar-novoross.ru/"
IFRAME_CSS = "iframe[src*='google.com/maps']"


async def map_is_correct_height(client):
    await client.navigate(URL)
    iframe = client.await_css(IFRAME_CSS)
    assert iframe
    return client.execute_script(
        """
       const iframe = arguments[0];
       return iframe.clientHeight == iframe.parentNode.clientHeight;
    """,
        iframe,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await map_is_correct_height(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await map_is_correct_height(client)
