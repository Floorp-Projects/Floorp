/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that <video>'s with styles on the element don't have those
 * styles cloned over into the <video> that's inserted into the
 * player window.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      let styles = {
        padding: "15px",
        border: "5px solid red",
        margin: "3px",
        position: "absolute",
      };

      await SpecialPowers.spawn(browser, [styles], async videoStyles => {
        let video = content.document.getElementById("no-controls");
        for (let styleProperty in videoStyles) {
          video.style[styleProperty] = videoStyles[styleProperty];
        }
      });

      let pipWin = await triggerPictureInPicture(browser, "no-controls");
      ok(pipWin, "Got Picture-in-Picture window.");

      let playerBrowser = pipWin.document.getElementById("browser");
      await SpecialPowers.spawn(playerBrowser, [styles], async videoStyles => {
        let video = content.document.querySelector("video");
        for (let styleProperty in videoStyles) {
          Assert.equal(
            video.style[styleProperty],
            "",
            `Style ${styleProperty} should not be set`
          );
        }
      });
      await BrowserTestUtils.closeWindow(pipWin);
    }
  );
});
