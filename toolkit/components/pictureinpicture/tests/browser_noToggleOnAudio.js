/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that if a <video> element only has audio, and no video
 * frames, that we do not show the toggle.
 */
add_task(async function test_no_toggle_on_audio() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ROOT + "owl.mp3",
    },
    async browser => {
      await ensureVideosReady(browser);
      await SimpleTest.promiseFocus(browser);

      // The media player document we create for owl.mp3 inserts a <video>
      // element pointed at the .mp3 file, which is what we're trying to
      // test for. The <video> element does not get an ID created for it
      // though, so we sneak one in with SpecialPowers.spawn so that we
      // can use testToggleHelper (which requires an ID).
      //
      // testToggleHelper also wants click-event-helper.js loaded in the
      // document, so we insert that too.
      const VIDEO_ID = "video-element";
      const SCRIPT_SRC = "click-event-helper.js";
      await SpecialPowers.spawn(browser, [VIDEO_ID, SCRIPT_SRC], async function(
        videoID,
        scriptSrc
      ) {
        let video = content.document.querySelector("video");
        video.id = videoID;

        let script = content.document.createElement("script");
        script.src = scriptSrc;
        content.document.head.appendChild(script);
      });

      await testToggleHelper(browser, VIDEO_ID, false);
    }
  );
});
