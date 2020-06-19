"use strict";

function openIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    window,
    "popupshown",
    true,
    event => event.target == gIdentityHandler._identityPopup
  );
  gIdentityHandler._identityBox.click();
  return promise;
}

function closeIdentityPopup() {
  let promise = BrowserTestUtils.waitForEvent(
    gIdentityHandler._identityPopup,
    "popuphidden"
  );
  gIdentityHandler._identityPopup.hidePopup();
  return promise;
}

// This test checks applied WebExtension themes that attempt to change
// popup properties

add_task(async function test_popup_styling(browser, accDoc) {
  const POPUP_BACKGROUND_COLOR = "#FF0000";
  const POPUP_TEXT_COLOR = "#008000";
  const POPUP_BORDER_COLOR = "#0000FF";

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        images: {
          theme_frame: "image1.png",
        },
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          popup: POPUP_BACKGROUND_COLOR,
          popup_text: POPUP_TEXT_COLOR,
          popup_border: POPUP_BORDER_COLOR,
        },
      },
    },
    files: {
      "image1.png": BACKGROUND,
    },
  });

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "https://example.com" },
    async function(browser) {
      await extension.startup();

      // Open the information arrow panel
      await openIdentityPopup();

      let arrowContent = gIdentityHandler._identityPopup.shadowRoot.querySelector(
        ".panel-arrowcontent"
      );
      let arrowContentComputedStyle = window.getComputedStyle(arrowContent);
      // Ensure popup background color was set properly
      Assert.equal(
        arrowContentComputedStyle.getPropertyValue("background-color"),
        `rgb(${hexToRGB(POPUP_BACKGROUND_COLOR).join(", ")})`,
        "Popup background color should have been themed"
      );

      // Ensure popup text color was set properly
      Assert.equal(
        arrowContentComputedStyle.getPropertyValue("color"),
        `rgb(${hexToRGB(POPUP_TEXT_COLOR).join(", ")})`,
        "Popup text color should have been themed"
      );

      // Ensure popup border color was set properly
      if (AppConstants.platform == "macosx") {
        Assert.ok(
          arrowContentComputedStyle
            .getPropertyValue("box-shadow")
            .includes(`rgb(${hexToRGB(POPUP_BORDER_COLOR).join(", ")})`),
          "Popup border color should be set"
        );
      } else {
        testBorderColor(arrowContent, POPUP_BORDER_COLOR);
      }

      await closeIdentityPopup();
      await extension.unload();
    }
  );
});
