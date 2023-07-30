import pytest
from webdriver.error import NoSuchElementException

URL = "https://www.vivobarefoot.com/eu/mens"
FILTER_CSS = "#narrow-by-list .filter-wrapper:last-of-type dt"
SUBMENU_CSS = "#narrow-by-list .filter-wrapper:last-of-type dd"
POPUP1_CSS = "#globalePopupWrapper"
POPUP2_CSS = "#globale_overlay"
POPUP3_CSS = ".weblayer--box-subscription-1"


async def check_filter_opens(client):
    await client.navigate(URL)

    popup = client.await_css(POPUP1_CSS, timeout=3)
    if popup:
        client.remove_element(popup)
    popup = client.find_css(POPUP2_CSS)
    if popup:
        client.remove_element(popup)
    popup = client.find_css(POPUP3_CSS)
    if popup:
        client.remove_element(popup)

    filter = client.await_css(FILTER_CSS)

    # we need to wait for the page to add the click listener
    client.execute_async_script(
        """
        const filter = arguments[0];
        const resolve = arguments[1];
        const ETP = EventTarget.prototype;
        const AEL = ETP.addEventListener;
        ETP.addEventListener = function(type) {
          if (this === filter && type === "click") {
            resolve();
          }
          return AEL.apply(this, arguments);
        };
    """,
        filter,
    )

    client.mouse.click(element=filter).perform()
    try:
        client.await_css(SUBMENU_CSS, is_displayed=True, timeout=3)
    except NoSuchElementException:
        return False
    return True


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await check_filter_opens(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await check_filter_opens(client)
