/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

var MockFilePicker = SpecialPowers.MockFilePicker;
MockFilePicker.init(window);
registerCleanupFunction(() => MockFilePicker.cleanup());

/**
 * Check downloading files URL-escapes content-disposition
 * information when necessary.
 */
add_task(async function test_check_filename_urlescape() {
  let pendingPromise;
  let pendingTest = "";
  let expectedFileName = "";
  MockFilePicker.showCallback = function (fp) {
    info(`${pendingTest} - Filepicker shown, checking filename`);
    is(
      fp.defaultString,
      expectedFileName,
      `${pendingTest} - Should have escaped filename`
    );
    ok(
      pendingPromise,
      `${pendingTest} - Should have expected this picker open.`
    );
    if (pendingPromise) {
      pendingPromise.resolve();
    }
    return Ci.nsIFilePicker.returnCancel;
  };
  function runTestFor(fileName, selector) {
    return BrowserTestUtils.withNewTab(TEST_PATH + fileName, async browser => {
      expectedFileName = fileName;
      let tabLabel = gBrowser.getTabForBrowser(browser).getAttribute("label");
      ok(
        tabLabel.startsWith(fileName),
        `"${tabLabel}" should have been escaped.`
      );

      pendingTest = "save browser";
      pendingPromise = Promise.withResolvers();
      // First try to save the browser
      saveBrowser(browser);
      await pendingPromise.promise;

      // Next, try the context menu:
      pendingTest = "save from context menu";
      pendingPromise = Promise.withResolvers();
      let menu = document.getElementById("contentAreaContextMenu");
      let menuShown = BrowserTestUtils.waitForEvent(menu, "popupshown");
      BrowserTestUtils.synthesizeMouse(
        selector,
        5,
        5,
        { type: "contextmenu", button: 2 },
        browser
      );
      await menuShown;
      gContextMenu.saveMedia();
      menu.hidePopup();
      await pendingPromise.promise;
      pendingPromise = null;
    });
  }
  await runTestFor("file_with@@funny_name.png", "img");
  await runTestFor("file_with[funny_name.webm", "video");
});
