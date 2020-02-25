/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that videos hosted inside of a third-party <iframe> can be opened
 * in a Picture-in-Picture window.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: TEST_PAGE_WITH_IFRAME,
        gBrowser,
      },
      async browser => {
        // TEST_PAGE_WITH_IFRAME is hosted at a different domain from TEST_PAGE,
        // so loading TEST_PAGE within the iframe will act as our third-party
        // iframe.
        await SpecialPowers.spawn(browser, [TEST_PAGE], async TEST_PAGE => {
          let iframe = content.document.getElementById("iframe");
          let loadPromise = ContentTaskUtils.waitForEvent(iframe, "load");
          iframe.src = TEST_PAGE;
          await loadPromise;
        });

        let iframeBc = browser.browsingContext.children[0];
        let pipWin = await triggerPictureInPicture(iframeBc, videoID);
        ok(pipWin, "Got Picture-in-Picture window.");

        try {
          await assertShowingMessage(iframeBc, videoID, true);
        } finally {
          let uaWidgetUpdate = SpecialPowers.spawn(iframeBc, [], async () => {
            await ContentTaskUtils.waitForEvent(
              content.windowRoot,
              "UAWidgetSetupOrChange",
              true /* capture */
            );
          });
          await BrowserTestUtils.closeWindow(pipWin);
          await uaWidgetUpdate;
        }
      }
    );
  }
});
