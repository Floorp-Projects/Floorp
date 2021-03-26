"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the color of the tab line are applied properly.

add_task(async function test_support_tab_line() {
  for (let protonTabsEnabled of [true, false]) {
    SpecialPowers.pushPrefEnv({
      set: [["browser.proton.tabs.enabled", protonTabsEnabled]],
    });
    let newWin = await BrowserTestUtils.openNewWindowWithFlushedXULCacheForMozSupports();

    const TAB_LINE_COLOR = "#9400ff";
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        theme: {
          colors: {
            frame: ACCENT_COLOR,
            tab_background_text: TEXT_COLOR,
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
    if (protonTabsEnabled) {
      Assert.equal(
        newWin.getComputedStyle(line).display,
        "none",
        "Tab line should not be displayed when Proton is enabled"
      );
    } else {
      Assert.equal(
        newWin.getComputedStyle(line).backgroundColor,
        `rgb(${hexToRGB(TAB_LINE_COLOR).join(", ")})`,
        "Tab line should have theme color"
      );
    }

    await extension.unload();
    await BrowserTestUtils.closeWindow(newWin);
  }
});
