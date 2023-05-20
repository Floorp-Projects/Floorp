/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

/**
 * Test that the reader mode correctly calculates and displays the
 * estimated reading time for a short article
 */
add_task(async function () {
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "readerModeArticleShort.html",
    async function (browser) {
      let pageShownPromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "AboutReaderContentReady"
      );
      let readerButton = document.getElementById("reader-mode-button");
      readerButton.click();
      await pageShownPromise;
      await SpecialPowers.spawn(browser, [], async function () {
        function dispatchMouseEvent(win, target, eventName) {
          let mouseEvent = new win.MouseEvent(eventName, {
            view: win,
            bubbles: true,
            cancelable: true,
            composed: true,
          });
          target.dispatchEvent(mouseEvent);
        }

        function simulateClick(target) {
          dispatchMouseEvent(win, target, "mousedown");
          dispatchMouseEvent(win, target, "mouseup");
          dispatchMouseEvent(win, target, "click");
        }

        let doc = content.document;
        let win = content.window;
        let styleButton = doc.querySelector(".style-button");

        let styleDropdown = doc.querySelector(".style-dropdown");
        ok(!styleDropdown.classList.contains("open"), "dropdown is closed");

        simulateClick(styleButton);
        ok(styleDropdown.classList.contains("open"), "dropdown is open");

        // simulate clicking on the article title to close the dropdown
        let title = doc.querySelector("h1");
        simulateClick(title);
        ok(!styleDropdown.classList.contains("open"), "dropdown is closed");

        // reopen the dropdown
        simulateClick(styleButton);
        ok(styleDropdown.classList.contains("open"), "dropdown is open");

        // now click on the button again to close it
        simulateClick(styleButton);
        ok(!styleDropdown.classList.contains("open"), "dropdown is closed");
      });
    }
  );
});
