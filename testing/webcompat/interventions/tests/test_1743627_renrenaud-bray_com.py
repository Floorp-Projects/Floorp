import pytest

URL = "https://www.renaud-bray.com/accueil.aspx"
ERROR_MSG = "ua.split(...)[1] is undefined"
MENU_CSS = "#pageHeader_menuStores"


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert client.execute_script(
        """
        return arguments[0].style.zIndex == "7000";
    """,
        client.await_css(MENU_CSS),
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL, await_console_message=ERROR_MSG)
