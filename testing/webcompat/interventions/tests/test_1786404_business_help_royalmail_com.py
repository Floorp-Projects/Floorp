import time

import pytest
from webdriver.error import StaleElementReferenceException

URL = "https://business.help.royalmail.com/app/webforms/stampsenquiries"

COOKIES_ACCEPT_CSS = "#consent_prompt_submit"
POSTCODE_CSS = "input[name='rn_AddressControl_16textbox']"
OPTION_CSS = "option.addr_pick_line:not(:disabled)"
ADDY1_CSS = "[name='Incident.CustomFields.c.address1_1']"


async def getResult(client):
    await client.navigate(URL)

    client.await_css(COOKIES_ACCEPT_CSS).click()

    # the viewport scrolls around while we try to interact
    # with the postcode in webdriver, so we take care here
    code = client.await_css(POSTCODE_CSS)
    client.scroll_into_view(code)
    time.sleep(1)
    client.mouse.click(element=code).perform()
    code.send_keys("W1A 1AV")

    # now wait for the options to open, scroll them back
    # into view, and carefully click on the first one.
    time.sleep(1)
    client.scroll_into_view(code)
    opt = client.await_css(OPTION_CSS)
    client.mouse.click(element=opt).perform()

    # the options should hide on click
    time.sleep(2)
    try:
        if client.is_displayed(opt):
            return False
    except StaleElementReferenceException:
        return False

    # a address line should end up being prefilled
    addy = client.await_css(ADDY1_CSS)
    return client.execute_async_script(
        """
        var addy = arguments[0];
        var done = arguments[1];
        var to = setTimeout(() => {
          if (addy.value) {
            clearTimeout(to);
            done(true);
          }
        }, 200);
      """,
        addy,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await getResult(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await getResult(client)
