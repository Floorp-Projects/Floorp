"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the background color of selected tab are applied correctly.

add_task(async function test_tab_background_color_property() {
  const TAB_BACKGROUND_COLOR = "#9400ff";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      theme: {
        colors: {
          frame: ACCENT_COLOR,
          tab_background_text: TEXT_COLOR,
          tab_selected: TAB_BACKGROUND_COLOR,
        },
      },
    },
  });

  await extension.startup();

  info("Checking selected tab color");

  let openTab = document.querySelector(
    ".tabbrowser-tab[visuallyselected=true]"
  );
  let openTabBackground = openTab.querySelector(".tab-background");

  let selectedTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:blank"
  );
  let selectedTabBackground = selectedTab.querySelector(".tab-background");

  let openTabGradient = window
    .getComputedStyle(openTabBackground)
    .getPropertyValue("background-image");
  let selectedTabGradient = window
    .getComputedStyle(selectedTabBackground)
    .getPropertyValue("background-image");

  let rgbRegex = /rgb\((\d{1,3}), (\d{1,3}), (\d{1,3})\)/g;
  let selectedTabColors = selectedTabGradient.match(rgbRegex);

  Assert.equal(
    selectedTabColors[0],
    "rgb(" + hexToRGB(TAB_BACKGROUND_COLOR).join(", ") + ")",
    "Selected tab background color should be set."
  );
  Assert.equal(openTabGradient, "none");

  gBrowser.removeTab(selectedTab);
  await extension.unload();
});
