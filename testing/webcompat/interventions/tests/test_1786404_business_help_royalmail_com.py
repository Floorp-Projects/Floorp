import pytest

URL = "https://business.help.royalmail.com/app/webforms/stampsenquiries"

COOKIES_ACCEPT_CSS = "#consent_prompt_submit"
POSTCODE_CSS = "input[name='rn_AddressControl_16textbox']"
OPTION_CSS = "option.addr_pick_line:not(:disabled)"
ADDY1_CSS = "[name='Incident.CustomFields.c.address1_1']"


async def getResult(client):
    await client.navigate(URL)
    client.await_css(COOKIES_ACCEPT_CSS).click()
    client.execute_script(
        f"""
      const proto = EventTarget.prototype;
      const def = Object.getOwnPropertyDescriptor(proto, "addEventListener");
      const old = def.value;
      def.value = function(type) {{
        if (type === "click" && this?.matches("{OPTION_CSS}")) {{
          window.__expectedListenerAdded = true;
        }}
      }};
      Object.defineProperty(proto, "addEventListener", def);
    """
    )

    client.await_css(POSTCODE_CSS).send_keys("W1A 1AV")
    client.await_css(OPTION_CSS)
    return client.execute_script(
        """
        return Boolean(window.__expectedListenerAdded);
    """
    )


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await getResult(client)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await getResult(client)
