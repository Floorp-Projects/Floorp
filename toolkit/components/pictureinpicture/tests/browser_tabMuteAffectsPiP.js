/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the tab's mute state is reflected in PiP when:
 * 1. PiP is first opened.
 * 2. Muting/unmuting the tab while PiP is open.
 * The tab can control the behavior of its PiP window,
 * but the PiP window cannot control the tab's behavior.
 */

add_task(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.audio-toggle.enabled", true],
    ],
  });
  let videoID = "with-controls";
  info(`Testing ${videoID} case.`);

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      async function toggleTabMute(tab, shouldMute) {
        let contextMenu = document.getElementById("tabContextMenu");
        let popupShownPromise = BrowserTestUtils.waitForEvent(
          contextMenu,
          "popupshown"
        );
        EventUtils.synthesizeMouseAtCenter(tab, {
          type: "contextmenu",
          button: 2,
        });
        await popupShownPromise;
        let toggleMute = document.getElementById("context_toggleMuteTab");

        EventUtils.synthesizeMouseAtCenter(toggleMute, {});

        await BrowserTestUtils.waitForCondition(
          () => !!tab.getAttribute("muted") === shouldMute
        );
        ok(true, "The tab mute was toggled correctly.");
      }

      let waitForVideoEvent = eventType => {
        return BrowserTestUtils.waitForContentEvent(browser, eventType, true);
      };

      // Set tab to muted
      let tab = gBrowser.getTabForBrowser(browser);
      await toggleTabMute(tab, true);

      await ensureVideosReady(browser);

      // Open the video in PiP
      let pipWin = await triggerPictureInPicture(browser, videoID);

      // Check if PiP audio is muted
      ok(await isVideoMuted(browser, videoID), "The audio is muted.");

      // Click the audio button
      let mutedPromise = waitForVideoEvent("volumechange");
      let audioButton = pipWin.document.getElementById("audio");
      EventUtils.synthesizeMouseAtCenter(audioButton, {}, pipWin);

      // Check if PiP audio is unmuted
      await mutedPromise;
      ok(!(await isVideoMuted(browser, videoID)), "The audio is unmuted.");

      // Check that clicking the audio button did not change the tab's state
      ok(tab.getAttribute("muted"), "The tab is still muted.");

      // Unmute and remute the tab
      mutedPromise = waitForVideoEvent("volumechange");
      await toggleTabMute(tab, false);
      await toggleTabMute(tab, true);

      // Check if PiP audio is muted
      await mutedPromise;
      ok(await isVideoMuted(browser, videoID), "The audio is muted.");
    }
  );
});
