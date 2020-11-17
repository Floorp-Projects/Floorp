/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * The goal of this test is to check that the icon on the PiP button mirrors
 * and the explainer text that shows up before the first time PiP is used
 * right aligns when the browser is set to a RtL mode
 *
 * The browser will create a tab and open a video using PiP
 * then the tests check that the components change their appearance accordingly
 *
 */

/**
 * This test ensures that the default ltr is working as intended
 */
add_task(async function test_ltr_toggle() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await ensureVideosReady(browser);
      for (let videoId of ["with-controls", "no-controls"]) {
        let localeDir = await SpecialPowers.spawn(browser, [videoId], id => {
          let video = content.document.getElementById(id);
          let videocontrols = video.openOrClosedShadowRoot.firstChild;
          return videocontrols.getAttribute("localedir");
        });

        Assert.equal(localeDir, "ltr", "Got the right localedir");
      }
    }
  );
});

/**
 * This test ensures that the components flip correctly when rtl is set
 */
add_task(async function test_rtl_toggle() {
  await SpecialPowers.pushPrefEnv({
    set: [["intl.l10n.pseudo", "bidi"]],
  });

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await ensureVideosReady(browser);
      for (let videoId of ["with-controls", "no-controls"]) {
        let localeDir = await SpecialPowers.spawn(browser, [videoId], id => {
          let video = content.document.getElementById(id);
          let videocontrols = video.openOrClosedShadowRoot.firstChild;
          return videocontrols.getAttribute("localedir");
        });

        Assert.equal(localeDir, "rtl", "Got the right localedir");
      }
    }
  );
});
