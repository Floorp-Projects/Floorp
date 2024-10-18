import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.six-group.com/en/market-data/etf/etf-explorer.html"
COOKIES_CSS = "#onetrust-consent-sdk"
SELECT_CSS = "select[name='Dropdown']:has(option[value='active'])"


async def select_value_is_visible(client):
    try:
        cookies = client.await_css(COOKIES_CSS, is_displayed=True, timeout=5)
        client.remove_element(cookies)
    except NoSuchElementException:
        pass

    select = client.await_css(SELECT_CSS, is_displayed=True)
    assert select
    # remove the border and background from the select, so only the text
    # should influence the screenshot.
    client.execute_script(
        """
        arguments[0].style.border = 'none';
        arguments[0].style.background = '#fff';
    """,
        select,
    )
    return not client.is_one_solid_color(select)


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert await select_value_is_visible(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert not await select_value_is_visible(client)
