/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 *  Tests that the PiP toggle button is not flipped
 *  on certain websites (such as whereby.com).
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ROOT + "test-reversed.html",
    },
    async browser => {
      await ensureVideosReady(browser);

      let videoID = "reversed";

      // Test the toggle button
      await prepareForToggleClick(browser, videoID);

      // Hover the mouse over the video to reveal the toggle.
      await BrowserTestUtils.synthesizeMouseAtCenter(
        `#${videoID}`,
        {
          type: "mousemove",
        },
        browser
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        `#${videoID}`,
        {
          type: "mouseover",
        },
        browser
      );

      let toggleFlippedAttribute = await SpecialPowers.spawn(
        browser,
        [videoID],
        async videoID => {
          let video = content.document.getElementById(videoID);
          let shadowRoot = video.openOrClosedShadowRoot;
          let controlsOverlay = shadowRoot.querySelector(".controlsOverlay");

          await ContentTaskUtils.waitForCondition(() => {
            return controlsOverlay.classList.contains("hovering");
          }, "Waiting for the hovering state to be set on the video.");

          return shadowRoot.firstChild.getAttribute("flipped");
        }
      );

      Assert.equal(
        toggleFlippedAttribute,
        "",
        `The "flipped" attribute should be set on the toggle button (when applicable).`
      );
    }
  );
});

/**
 * Tests that the "This video is playing in Picture-in-Picture" message
 * as well as the video playing in PiP are both not flipped on certain sites
 * (such as whereby.com)
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ROOT + "test-reversed.html",
    },
    async browser => {
      /**
       * A helper function used to get the "flipped" attribute of the video's shadowRoot's first child.
       * @param {Element} browser The <xul:browser> hosting the <video>
       * @param {String} videoID The ID of the video being checked
       */
      async function getFlippedAttribute(browser, videoID) {
        let videoFlippedAttribute = await SpecialPowers.spawn(
          browser,
          [videoID],
          async videoID => {
            let video = content.document.getElementById(videoID);
            let shadowRoot = video.openOrClosedShadowRoot;
            return shadowRoot.firstChild.getAttribute("flipped");
          }
        );
        return videoFlippedAttribute;
      }

      /**
       * A helper function that returns the transform.a of the video being played in PiP.
       * @param {Element} playerBrowser The <xul:browser> of the PiP window
       */
      async function getPiPVideoTransform(playerBrowser) {
        let pipVideoTransform = await SpecialPowers.spawn(
          playerBrowser,
          [],
          async () => {
            let video = content.document.querySelector("video");
            return video.getTransformToViewport().a;
          }
        );
        return pipVideoTransform;
      }

      await ensureVideosReady(browser);

      let videoID = "reversed";

      let videoFlippedAttribute = await getFlippedAttribute(browser, videoID);
      Assert.equal(
        videoFlippedAttribute,
        null,
        `The "flipped" attribute should not be set initially.`
      );

      let pipWin = await triggerPictureInPicture(browser, videoID);

      videoFlippedAttribute = await getFlippedAttribute(browser, "reversed");
      Assert.equal(
        videoFlippedAttribute,
        "true",
        `The "flipped" value should be set once the PiP window is opened (when applicable).`
      );

      let playerBrowser = pipWin.document.getElementById("browser");
      let pipVideoTransform = await getPiPVideoTransform(playerBrowser);
      Assert.equal(pipVideoTransform, -1);

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);

      videoFlippedAttribute = await getFlippedAttribute(browser, "reversed");
      Assert.equal(
        videoFlippedAttribute,
        null,
        `The "flipped" attribute should be removed after closing PiP.`
      );

      // Now we want to test that regular (not-reversed) videos are unaffected
      videoID = "not-reversed";
      videoFlippedAttribute = await getFlippedAttribute(browser, videoID);
      Assert.equal(videoFlippedAttribute, null);

      pipWin = await triggerPictureInPicture(browser, videoID);

      videoFlippedAttribute = await getFlippedAttribute(browser, videoID);
      Assert.equal(videoFlippedAttribute, null);

      playerBrowser = pipWin.document.getElementById("browser");
      pipVideoTransform = await getPiPVideoTransform(playerBrowser);

      await ensureMessageAndClosePiP(browser, videoID, pipWin, false);
    }
  );
});
