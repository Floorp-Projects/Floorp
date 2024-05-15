/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

async function testTextLayout(aPref, value, cssProp, cssValue) {
  // Enable the improved text and layout menu.
  Services.prefs.setBoolPref("reader.improved_text_menu.enabled", true);

  // Set the pref to the custom value.
  const valueType = typeof value;
  if (valueType == "number") {
    Services.prefs.setIntPref(`reader.${aPref}`, value);
  } else if (valueType == "string") {
    Services.prefs.setCharPref(`reader.${aPref}`, value);
  }

  // Open a browser tab, enter reader mode, and test if the page layout
  // reflects the pref selection.
  await BrowserTestUtils.withNewTab(
    TEST_PATH + "readerModeArticle.html",
    async function (browser) {
      let pageShownPromise = BrowserTestUtils.waitForContentEvent(
        browser,
        "AboutReaderContentReady"
      );

      let readerButton = document.getElementById("reader-mode-button");
      readerButton.click();
      await pageShownPromise;

      let customPref;
      if (valueType == "number") {
        customPref = Services.prefs.getIntPref(`reader.${aPref}`);
      } else if (valueType == "string") {
        customPref = Services.prefs.getCharPref(`reader.${aPref}`);
      }
      Assert.equal(customPref, value);

      await SpecialPowers.spawn(
        browser,
        [cssValue, cssProp],
        (expectedValue, prop) => {
          let container = content.document.querySelector(".container");
          let style = content.window.getComputedStyle(container);
          let actualValue = style.getPropertyValue(`--${prop}`);
          Assert.equal(actualValue, expectedValue);
        }
      );
    }
  );
}

/**
 * Test that the layout and advanced layout pref selection updates
 * the document layout correctly.
 */
add_task(async function () {
  await testTextLayout("font_size", 7, "font-size", "24px");
  await testTextLayout(
    "font_type",
    "monospace",
    "font-family",
    '"Courier New", Courier, monospace'
  );
  await testTextLayout("font_weight", "bold", "font-weight", "bolder");
  await testTextLayout("content_width", 7, "content-width", "50em");
  await testTextLayout("line_height", 7, "line-height", "2.2em");
  await testTextLayout("character_spacing", 7, "letter-spacing", "0.18em");
  await testTextLayout("word_spacing", 7, "word-spacing", "0.30em");
  await testTextLayout("text_alignment", "right", "text-alignment", "right");
});
