"use strict";

// This test checks whether applied WebExtension themes that attempt to change
// the color of the tab line are applied properly.

add_task(async function test_support_tab_line() {
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
  let selectedTab = newWin.document.querySelector(".tabbrowser-tab[selected]");
  let tab = selectedTab.querySelector(".tab-background");
  let element = tab;
  let property = "boxShadow";
  let computedValue = newWin.getComputedStyle(element)[property];
  let expectedColor = `rgb(${hexToRGB(TAB_LINE_COLOR).join(", ")})`;
  Assert.ok(
    computedValue.includes(expectedColor),
    `Tab line should be displayed in the box shadow of the tab: ${computedValue}`
  );

  await extension.unload();
  await BrowserTestUtils.closeWindow(newWin);
});
