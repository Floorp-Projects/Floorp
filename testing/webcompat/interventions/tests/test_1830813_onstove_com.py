import pytest

URL = "https://page.onstove.com/epicseven/global/"
BANNER_CSS = ".gnb-alerts.gnb-old-browser"


async def get_banner_height(client):
    await client.navigate(URL)
    banner = client.await_css(BANNER_CSS)
    return client.execute_script(
        """
       return arguments[0].getBoundingClientRect().height;
    """,
        banner,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert 0 == await get_banner_height(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert 0 < await get_banner_height(client)
