import pytest

URL = "https://www.91mobiles.com/compare/realme/9+SE+5G/vs/Moto/G72-amp"
CONTENT_CSS = "#fixed-table tr td .cmp-summary-box"


async def content_has_height(client):
    await client.navigate(URL)
    elem = client.await_css(CONTENT_CSS)
    return client.execute_script(
        """
      return arguments[0].getBoundingClientRect().height > 0;
    """,
        elem,
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await content_has_height(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await content_has_height(client)
