import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.axa-assistance.pl/ubezpieczenie-turystyczne/?externalpartner=13100009&gclid=Cj0KCQjw4NujBhC5ARIsAF4Iv6eUnfuIEl9YkvO6pXP-I8g0ImynOMqpS7eMdBhyhjOj7G4eZzfSr_oaAnEUEALw_wcB#"
DATE_CSS = ".hiddenDate"
COOKIES_CSS = "#onetrust-accept-btn-handler"
MIN_WIDTH = 100


def get_elem_width(client):
    try:
        client.soft_click(client.await_css(COOKIES_CSS))
        client.await_element_hidden(client.css(COOKIES_CSS))
    except NoSuchElementException:
        pass

    elem = client.await_css(DATE_CSS)
    return client.execute_script(
        """
        return arguments[0].getBoundingClientRect().width;
    """,
        elem,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    assert get_elem_width(client) > MIN_WIDTH


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert get_elem_width(client) < MIN_WIDTH
