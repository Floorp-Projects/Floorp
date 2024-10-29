import pytest

URL = "https://docs.google.com/document/d/1MUJpu1E-wR1Gq-a8csdzXbOXf4mSzNuMQGxoPPBqK1w/edit"

FONT_BUTTON_CSS = "#docs-font-family .goog-toolbar-menu-button-inner-box"
FONT_MENU_WITH_SUBMENU_CSS = (
    ".goog-menu-vertical.docs-fontmenu .docs-submenuitem:has(.goog-submenu-arrow)"
)


async def are_font_submenus_accessible(client):
    await client.navigate(URL)

    # wait for the submenus to be ready (added to the DOM)
    client.await_css(FONT_MENU_WITH_SUBMENU_CSS)

    # open a submenu
    client.await_css(FONT_BUTTON_CSS, is_displayed=True).click()
    menuitem = client.await_css(FONT_MENU_WITH_SUBMENU_CSS, is_displayed=True)
    font = client.execute_script(
        """
        const [menuitem] = arguments;

        // get the name of the font, which helps us know which panel to wait for docs to create
        const font = menuitem.querySelector("span[style]").style.fontFamily.replace("--Menu", "");

        // trigger a mouseover on the menu item so docs opens the panel
        menuitem.dispatchEvent(new MouseEvent("mouseover", {
          bubbles: true,
          cancelable: true,
          view: window,
        }));

        return font;
    """,
        menuitem,
    )

    # wait for the on-hover popup to actually be created and displayed
    popup = client.await_xpath(
        "//*[contains(@style, '{}')]".format(font), is_displayed=True
    )
    assert popup

    return client.execute_script(
        """
        const menuitem_arrow = arguments[0].querySelector(".goog-submenu-arrow");
        const popup = arguments[1].closest(".goog-menu.goog-menu-vertical");
        return popup.getBoundingClientRect().left < menuitem_arrow.getBoundingClientRect().right;
    """,
        menuitem,
        popup,
    )


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    assert await are_font_submenus_accessible(client)


@pytest.mark.skip_platforms("android")
@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    assert not await are_font_submenus_accessible(client)
