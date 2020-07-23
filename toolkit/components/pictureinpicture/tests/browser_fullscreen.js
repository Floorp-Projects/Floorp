/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const VIDEOS = ["with-controls", "no-controls"];

/**
 * Tests that the Picture-in-Picture toggle is hidden when
 * a video with or without controls is made fullscreen.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      for (let videoID of VIDEOS) {
        await promiseFullscreenEntered(window, async () => {
          await SpecialPowers.spawn(browser, [videoID], async videoID => {
            let video = this.content.document.getElementById(videoID);
            video.requestFullscreen();
          });
        });

        await BrowserTestUtils.synthesizeMouseAtCenter(
          `#${videoID}`,
          {
            type: "mouseover",
          },
          browser
        );

        let args = { videoID, toggleID: DEFAULT_TOGGLE_STYLES.rootID };

        await promiseFullscreenExited(window, async () => {
          await SpecialPowers.spawn(browser, [args], async args => {
            let { videoID, toggleID } = args;
            let video = this.content.document.getElementById(videoID);
            let toggle = video.openOrClosedShadowRoot.getElementById(toggleID);
            ok(
              ContentTaskUtils.is_hidden(toggle),
              "Toggle should be hidden in fullscreen mode."
            );
            this.content.document.exitFullscreen();
          });
        });
      }
    }
  );
});

/**
 * Tests that the Picture-in-Picture toggle is hidden if an
 * ancestor of a video (in this case, the document body) is made
 * to be the fullscreen element.
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await promiseFullscreenEntered(window, async () => {
        await SpecialPowers.spawn(browser, [], async () => {
          this.content.document.body.requestFullscreen();
        });
      });

      for (let videoID of VIDEOS) {
        await BrowserTestUtils.synthesizeMouseAtCenter(
          `#${videoID}`,
          {
            type: "mouseover",
          },
          browser
        );

        let args = { videoID, toggleID: DEFAULT_TOGGLE_STYLES.rootID };

        await SpecialPowers.spawn(browser, [args], async args => {
          let { videoID, toggleID } = args;
          let video = this.content.document.getElementById(videoID);
          let toggle = video.openOrClosedShadowRoot.getElementById(toggleID);
          ok(
            ContentTaskUtils.is_hidden(toggle),
            "Toggle should be hidden in fullscreen mode."
          );
        });
      }

      await promiseFullscreenExited(window, async () => {
        await SpecialPowers.spawn(browser, [], async () => {
          this.content.document.exitFullscreen();
        });
      });
    }
  );
});
