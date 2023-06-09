import pytest

URL = "https://www.eventer.co.il"
EVENT_CSS = "a.slide-a-tag[href*='/event']"
ELEM_CSS = ".mobileStripButton:not(.ng-hide)"


async def is_too_tall(client):
    await client.navigate(URL)
    event = client.await_css(EVENT_CSS)
    await client.navigate(URL + client.get_element_attribute(event, "href"))
    elem = client.await_css(ELEM_CSS)
    return client.execute_script(
        """
       return window.innerHeight == arguments[0].getBoundingClientRect().height;
    """,
        elem,
    )


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert not await is_too_tall(client)


@pytest.mark.only_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert await is_too_tall(client)
