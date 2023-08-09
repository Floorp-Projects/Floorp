/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  let tabs = [];
  for (let i = 0; i < 10; i++) {
    const tab = BrowserTestUtils.addTab(gBrowser);
    tabs.push(tab);
  }
  const kIsMac = AppConstants.platform == "macosx";

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      let NativeKeyConstants = {};
      Services.scriptloader.loadSubScript(
        "chrome://mochikit/content/tests/SimpleTest/NativeKeyCodes.js",
        NativeKeyConstants
      );

      function promiseSynthesizeAccelHyphenMinusWithAZERTY() {
        return new Promise(resolve =>
          EventUtils.synthesizeNativeKey(
            EventUtils.KEYBOARD_LAYOUT_FRENCH_PC,
            kIsMac
              ? NativeKeyConstants.MAC_VK_ANSI_6
              : NativeKeyConstants.WIN_VK_6,
            { accelKey: true },
            kIsMac ? "-" : "",
            kIsMac ? "-" : "",
            resolve
          )
        );
      }

      async function waitForCondition(aFunc) {
        for (let i = 0; i < 60; i++) {
          await new Promise(resolve =>
            requestAnimationFrame(() => requestAnimationFrame(resolve))
          );
          if (aFunc(ZoomManager.getFullZoomForBrowser(browser))) {
            return true;
          }
        }
        return false;
      }

      const minZoomLevel = ZoomManager.MIN;
      while (true) {
        const currentZoom = ZoomManager.getFullZoomForBrowser(browser);
        if (minZoomLevel == currentZoom) {
          break;
        }
        info(`Trying to zoom out: ${currentZoom}`);
        await promiseSynthesizeAccelHyphenMinusWithAZERTY();
        if (!(await waitForCondition(aZoomLevel => aZoomLevel < currentZoom))) {
          ok(false, `Failed to zoom out from ${currentZoom}`);
          return;
        }
      }

      await promiseSynthesizeAccelHyphenMinusWithAZERTY();
      await waitForCondition(() => false);
      is(
        gBrowser.selectedBrowser,
        browser,
        "Tab shouldn't be changed by Ctrl+- of AZERTY keyboard layout"
      );
      // Reset the zoom before going to the next test.
      EventUtils.synthesizeKey("0", { accelKey: true });
      await waitForCondition(aZoomLevel => aZoomLevel == 1);
    }
  );

  while (tabs.length) {
    await new Promise(resolve => {
      const tab = tabs.shift();
      BrowserTestUtils.waitForTabClosing(tab).then(resolve);
      BrowserTestUtils.removeTab(tab);
    });
  }
});
