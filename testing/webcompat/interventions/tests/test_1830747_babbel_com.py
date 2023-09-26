import pytest
from webdriver.error import NoSuchElementException

URL = "https://my.babbel.com/en/welcome/1?bsc=engmag-rus&btp=default&learn_lang=RUS"
COOKIES_CSS = "#onetrust-accept-btn-handler"
BUTTON_CSS = "button.button[type=submit]"


async def button_visible(client):
    await client.navigate(URL)
    try:
        client.soft_click(client.await_css(COOKIES_CSS, timeout=3))
    except NoSuchElementException:
        pass
    btn = client.await_css(BUTTON_CSS)
    return client.execute_script(
        """
       return window.innerHeight > arguments[0].getBoundingClientRect().bottom;
    """,
        btn,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await button_visible(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await button_visible(client)
