"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the color of the tab line are applied properly.

add_task(async function test_support_tab_line() {
  for (let protonTabsEnabled of [true, false]) {
    SpecialPowers.pushPrefEnv({
      set: [["browser.proton.enabled", protonTabsEnabled]],
    });
    let newWin = await BrowserTestUtils.openNewWindowWithFlushedXULCacheForMozSupports();

    const TAB_LINE_COLOR = "#ff0000";
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        theme: {
          colors: {
            frame: ACCENT_COLOR,
            tab_background_text: "#000",
            tab_line: TAB_LINE_COLOR,
          },
        },
      },
    });

    await extension.startup();

    info("Checking selected tab line color");
    let selectedTab = newWin.document.querySelector(
      ".tabbrowser-tab[selected]"
    );
    let line = selectedTab.querySelector(".tab-line");
    let tab = selectedTab.querySelector(".tab-background");
    let element = protonTabsEnabled ? tab : line;
    let property = protonTabsEnabled ? "boxShadow" : "backgroundColor";
    let computedValue = newWin.getComputedStyle(element)[property];
    let expectedColor = `rgb(${hexToRGB(TAB_LINE_COLOR).join(", ")})`;
    if (protonTabsEnabled) {
      Assert.ok(
        computedValue.includes(expectedColor),
        `Tab line should be displayed in the box shadow of the tab when Proton is enabled: ${computedValue}`
      );
    } else {
      Assert.equal(
        computedValue,
        expectedColor,
        "Tab line should have theme color"
      );
    }

    await extension.unload();
    await BrowserTestUtils.closeWindow(newWin);
  }
});
