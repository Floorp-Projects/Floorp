import pytest
from helpers import Css, await_element

URL = "https://business.help.royalmail.com/app/webforms/stampsenquiries"

COOKIES_ACCEPT_CSS = Css("#consent_prompt_submit")
POSTCODE_CSS = Css("input[name='rn_AddressControl_15textbox']")
OPTION_CSS = "option.addr_pick_line:not(:disabled)"
ADDY1_CSS = "[name='Incident.CustomFields.c.address1_1']"


def getResult(session):
    session.get(URL)
    await_element(session, COOKIES_ACCEPT_CSS).click()
    session.execute_script(
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

    await_element(session, POSTCODE_CSS).send_keys("W1A 1AV")
    await_element(session, Css(OPTION_CSS))
    return session.execute_script(
        """
        return Boolean(window.__expectedListenerAdded);
    """
    )


@pytest.mark.with_interventions
def test_enabled(session):
    assert getResult(session)


@pytest.mark.without_interventions
def test_disabled(session):
    assert not getResult(session)
