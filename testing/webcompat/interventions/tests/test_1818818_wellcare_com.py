import pytest

URL = "https://www.wellcare.com/en/Oregon/Members/Prescription-Drug-Plans-2023/Wellcare-Value-Script"
ALERT_CSS = ".alert-box radius"
MENU_CSS = "a[title='login DDL']"
SELECT_CSS = "select#userSelect"


async def is_fastclick_active(client):
    async with client.ensure_fastclick_activates():
        await client.navigate(URL, wait="load")
        alert = client.find_css(ALERT_CSS)
        if alert:
            client.remove_element(alert)
        menu = client.await_css(MENU_CSS)
        client.soft_click(menu)
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
