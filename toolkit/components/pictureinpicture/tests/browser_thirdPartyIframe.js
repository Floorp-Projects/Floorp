/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXAMPLE_COM = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const EXAMPLE_ORG = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.org"
);
const EXAMPLE_COM_TEST_PAGE = EXAMPLE_COM + "test-page.html";
const EXAMPLE_ORG_WITH_IFRAME = EXAMPLE_ORG + "test-page-with-iframe.html";

/**
 * Tests that videos hosted inside of a third-party <iframe> can be opened
 * in a Picture-in-Picture window.
 */
add_task(async () => {
  for (let videoID of ["with-controls", "no-controls"]) {
    info(`Testing ${videoID} case.`);

    await BrowserTestUtils.withNewTab(
      {
        url: EXAMPLE_ORG_WITH_IFRAME,
        gBrowser,
      },
      async browser => {
        // EXAMPLE_ORG_WITH_IFRAME is hosted at a different domain from
        // EXAMPLE_COM_TEST_PAGE, so loading EXAMPLE_COM_TEST_PAGE within
        // the iframe will act as our third-party iframe.
        await SpecialPowers.spawn(
          browser,
          [EXAMPLE_COM_TEST_PAGE],
          async EXAMPLE_COM_TEST_PAGE => {
            let iframe = content.document.getElementById("iframe");
            let loadPromise = ContentTaskUtils.waitForEvent(iframe, "load");
            iframe.src = EXAMPLE_COM_TEST_PAGE;
            await loadPromise;
          }
        );

        let iframeBc = browser.browsingContext.children[0];

        if (gFissionBrowser) {
          Assert.notEqual(
            browser.browsingContext.currentWindowGlobal.osPid,
            iframeBc.currentWindowGlobal.osPid,
            "The iframe should be running in a different process."
          );
        }

        let pipWin = await triggerPictureInPicture(iframeBc, videoID);
        ok(pipWin, "Got Picture-in-Picture window.");

        await ensureMessageAndClosePiP(iframeBc, videoID, pipWin, true);

        await SimpleTest.promiseFocus(window);

        // Now try using the command / keyboard shortcut
        pipWin = await triggerPictureInPicture(iframeBc, videoID, () => {
          document.getElementById("View:PictureInPicture").doCommand();
        });
        ok(pipWin, "Got Picture-in-Picture window using command.");

        await ensureMessageAndClosePiP(iframeBc, videoID, pipWin, true);
      }
    );
  }
});
