/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

async function testColorScheme(aPref, aExpectation) {
  // Set the browser content theme to light or dark.
  Services.prefs.setIntPref("browser.theme.content-theme", aPref);

  // Reader Mode Color Scheme Preference must be manually set by the user, will
  // default to "auto" initially.
  Services.prefs.setCharPref("reader.color_scheme", aExpectation);

  let aBodyExpectation = aExpectation;
  if (aBodyExpectation === "auto") {
    aBodyExpectation = aPref === 1 ? "light" : "dark";
  }

  // Open a browser tab, enter reader mode, and test if we have the valid
  // reader mode color scheme preference pre-selected.
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

      let colorScheme = Services.prefs.getCharPref("reader.color_scheme");

      Assert.equal(colorScheme, aExpectation);

      await SpecialPowers.spawn(browser, [aBodyExpectation], expectation => {
        let bodyClass = content.document.body.className;
        ok(
          bodyClass.includes(expectation),
          "The body of the test document has the correct color scheme."
        );
      });
    }
  );
}

/**
 * Test that opening reader mode maintains the correct color scheme preference
 * until the user manually sets a different color scheme.
 */
add_task(async function () {
  await testColorScheme(0, "auto");
  await testColorScheme(1, "auto");
  await testColorScheme(0, "light");
  await testColorScheme(1, "light");
  await testColorScheme(0, "dark");
  await testColorScheme(1, "dark");
  await testColorScheme(0, "sepia");
  await testColorScheme(1, "sepia");
});
