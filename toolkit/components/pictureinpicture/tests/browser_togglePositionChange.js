/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Tests that Picture-in-Picture "move toggle" context menu item
 * successfully changes preference.
 */
add_task(async () => {
  let videoID = "with-controls";
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);

      await SpecialPowers.spawn(browser, [videoID], async videoID => {
        await content.document.getElementById(videoID).play();
      });

      const TOGGLE_POSITION_PREF =
        "media.videocontrols.picture-in-picture.video-toggle.position";
      const TOGGLE_POSITION_RIGHT = "right";
      const TOGGLE_POSITION_LEFT = "left";

      await SpecialPowers.pushPrefEnv({
        set: [[TOGGLE_POSITION_PREF, TOGGLE_POSITION_RIGHT]],
      });

      let contextMoveToggle = document.getElementById(
        "context_MovePictureInPictureToggle"
      );
      contextMoveToggle.click();
      let position = Services.prefs.getStringPref(
        TOGGLE_POSITION_PREF,
        TOGGLE_POSITION_RIGHT
      );

      Assert.ok(
        position === TOGGLE_POSITION_LEFT,
        "Picture-in-Picture toggle position value should be 'left'."
      );

      contextMoveToggle.click();
      position = Services.prefs.getStringPref(
        TOGGLE_POSITION_PREF,
        TOGGLE_POSITION_RIGHT
      );

      Assert.ok(
        position === TOGGLE_POSITION_RIGHT,
        "Picture-in-Picture toggle position value should be 'right'."
      );
    }
  );
});
