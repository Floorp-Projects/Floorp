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
        info(`Start test of video fullscreen for video ${videoID}.`);
        await promiseFullscreenEntered(window, () => {
          return SpecialPowers.spawn(browser, [videoID], videoID => {
            let video = this.content.document.getElementById(videoID);
            return video.requestFullscreen();
          });
        });

        info(`Entered video fullscreen, about to mouseover the video.`);

        await BrowserTestUtils.synthesizeMouseAtCenter(
          `#${videoID}`,
          {
            type: "mouseover",
          },
          browser
        );

        info(`Mouseover complete.`);

        let args = { videoID, toggleID: DEFAULT_TOGGLE_STYLES.rootID };

        await promiseFullscreenExited(window, () => {
          return SpecialPowers.spawn(browser, [args], args => {
            let { videoID, toggleID } = args;
            let video = this.content.document.getElementById(videoID);
            let toggle = video.openOrClosedShadowRoot.getElementById(toggleID);
            ok(
              ContentTaskUtils.isHidden(toggle),
              `Toggle should be hidden in video fullscreen mode for video ${videoID}.`
            );
            return this.content.document.exitFullscreen();
          });
        });

        info(`Exited video fullscreen.`);
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
      info(`Start test of browser fullscreen.`);
      await promiseFullscreenEntered(window, () => {
        return SpecialPowers.spawn(browser, [], () => {
          return this.content.document.body.requestFullscreen();
        });
      });

      info(`Entered browser fullscreen.`);

      for (let videoID of VIDEOS) {
        info(`About to mouseover for video ${videoID}.`);

        await BrowserTestUtils.synthesizeMouseAtCenter(
          `#${videoID}`,
          {
            type: "mouseover",
          },
          browser
        );

        info(`Mouseover complete.`);

        let args = { videoID, toggleID: DEFAULT_TOGGLE_STYLES.rootID };

        await SpecialPowers.spawn(browser, [args], async args => {
          let { videoID, toggleID } = args;
          let video = this.content.document.getElementById(videoID);
          let toggle = video.openOrClosedShadowRoot.getElementById(toggleID);
          ok(
            ContentTaskUtils.isHidden(toggle),
            `Toggle should be hidden in body fullscreen mode for video ${videoID}.`
          );
        });
      }

      await promiseFullscreenExited(window, () => {
        return SpecialPowers.spawn(browser, [], () => {
          return this.content.document.exitFullscreen();
        });
      });

      info(`Exited browser fullscreen.`);
    }
  );
});

/**
 * Tests that the Picture-In-Picture window is closed when something
 * is fullscreened
 */
add_task(async () => {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_PAGE,
    },
    async browser => {
      await ensureVideosReady(browser);

      for (let videoID of VIDEOS) {
        let pipWin = await triggerPictureInPicture(browser, videoID);
        ok(pipWin, `Got Picture-In-Picture window for video ${videoID}.`);

        let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);

        // need to focus first, since fullscreen request will be blocked otherwise
        await SimpleTest.promiseFocus(window);

        await promiseFullscreenEntered(window, () => {
          return SpecialPowers.spawn(browser, [], () => {
            return this.content.document.body.requestFullscreen();
          });
        });

        await pipClosed;
        ok(
          pipWin.closed,
          `Picture-In-Picture successfully closed for video ${videoID}.`
        );

        await promiseFullscreenExited(window, () => {
          return SpecialPowers.spawn(browser, [], () => {
            return this.content.document.exitFullscreen();
          });
        });
      }
    }
  );
});
