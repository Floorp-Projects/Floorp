/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that Picture-in-Picture intializes without changes to video playback
 * when opened via the toggle using a touch event. Also ensures that elements
 * in the content window can still be interacted with afterwards.
 */
add_task(async () => {
  let videoID = "with-controls";

  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.videocontrols.picture-in-picture.video-toggle.position", "right"],
    ],
  });

  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);
      let toggleStyles = DEFAULT_TOGGLE_STYLES;
      let stage = "hoverVideo";
      let toggleStylesForStage = toggleStyles.stages[stage];

      // Remove other page elements before reading PiP toggle's client rect.
      // Otherwise, we will provide the wrong coordinates when simulating the touch event.
      await SpecialPowers.spawn(browser, [], async args => {
        info(
          "Removing other elements first to make the PiP toggle more visible"
        );
        this.content.document.getElementById("no-controls").remove();
        let otherEls = this.content.document.querySelectorAll("h1");
        for (let el of otherEls) {
          el.remove();
        }
      });

      let toggleClientRect = await getToggleClientRect(browser, videoID);
      await prepareForToggleClick(browser, videoID);

      await SpecialPowers.spawn(
        browser,
        [{ videoID, toggleClientRect, toggleStylesForStage }],
        async args => {
          // waitForToggleOpacity is based on toggleOpacityReachesThreshold.
          // Waits for toggle to reach target opacity.
          async function waitForToggleOpacity(
            shadowRoot,
            toggleStylesForStage
          ) {
            for (let hiddenElement of toggleStylesForStage.hidden) {
              let el = shadowRoot.querySelector(hiddenElement);
              ok(
                ContentTaskUtils.isHidden(el),
                `Expected ${hiddenElement} to be hidden.`
              );
            }

            for (let opacityElement in toggleStylesForStage.opacities) {
              let opacityThreshold =
                toggleStylesForStage.opacities[opacityElement];
              let el = shadowRoot.querySelector(opacityElement);

              await ContentTaskUtils.waitForCondition(
                () => {
                  let opacity = parseFloat(
                    this.content.getComputedStyle(el).opacity
                  );
                  return opacity >= opacityThreshold;
                },
                `Toggle element ${opacityElement} should have eventually reached ` +
                  `target opacity ${opacityThreshold}`,
                100,
                100
              );

              ok(true, "Toggle reached target opacity.");
            }
          }
          let { videoID, toggleClientRect, toggleStylesForStage } = args;
          let video = this.content.document.getElementById(videoID);
          let shadowRoot = video.openOrClosedShadowRoot;

          info("Creating a new button in the content window");
          let button = this.content.document.createElement("button");
          let buttonSelected = false;
          button.ontouchstart = () => {
            info("Button selected via touch event");
            buttonSelected = true;
            return true;
          };
          button.id = "testbutton";
          button.style.backgroundColor = "red";
          // Move button to the top for better visibility.
          button.style.position = "absolute";
          button.style.top = "0";
          button.textContent = "test button";
          this.content.document.body.appendChild(button);

          await video.play();

          info("Hover over the video to show the Picture-in-Picture toggle");
          await EventUtils.synthesizeMouseAtCenter(
            video,
            { type: "mousemove" },
            this.content.window
          );
          await EventUtils.synthesizeMouseAtCenter(
            video,
            { type: "mouseover" },
            this.content.window
          );

          let toggleCenterX =
            toggleClientRect.left + toggleClientRect.width / 2;
          let toggleCenterY =
            toggleClientRect.top + toggleClientRect.height / 2;

          // We want to wait for the toggle to reach opacity so that we can select it.
          info("Waiting for toggle to become fully visible");
          await waitForToggleOpacity(shadowRoot, toggleStylesForStage);

          info("Simulating touch event on PiP toggle");
          let utils = EventUtils._getDOMWindowUtils(this.content.window);
          let id = utils.DEFAULT_TOUCH_POINTER_ID;
          let rx = 1;
          let ry = 1;
          let angle = 0;
          let force = 1;
          let tiltX = 0;
          let tiltY = 0;
          let twist = 0;

          let defaultPrevented = utils.sendTouchEvent(
            "touchstart",
            [id],
            [toggleCenterX],
            [toggleCenterY],
            [rx],
            [ry],
            [angle],
            [force],
            [tiltX],
            [tiltY],
            [twist],
            false /* modifiers */
          );
          utils.sendTouchEvent(
            "touchend",
            [id],
            [toggleCenterX],
            [toggleCenterY],
            [rx],
            [ry],
            [angle],
            [force],
            [tiltX],
            [tiltY],
            [twist],
            false /* modifiers */
          );

          ok(
            defaultPrevented,
            "Touchstart event's default actions should be prevented"
          );
          ok(!video.paused, "Video should still be playing");

          await ContentTaskUtils.waitForCondition(() => {
            return video.isCloningElementVisually;
          }, "Video is being cloned visually.");

          let testButton = this.content.document.getElementById("testbutton");
          let buttonRect = testButton.getBoundingClientRect();
          let buttonCenterX = buttonRect.left + buttonRect.width / 2;
          let buttonCenterY = buttonRect.top + buttonRect.height / 2;

          info("Simulating touch event on new button");
          defaultPrevented = utils.sendTouchEvent(
            "touchstart",
            [id],
            [buttonCenterX],
            [buttonCenterY],
            [rx],
            [ry],
            [angle],
            [force],
            [tiltX],
            [tiltY],
            [twist],
            false /* modifiers */
          );
          utils.sendTouchEvent(
            "touchend",
            [id],
            [buttonCenterX],
            [buttonCenterY],
            [rx],
            [ry],
            [angle],
            [force],
            [tiltX],
            [tiltY],
            [twist],
            false /* modifiers */
          );

          ok(
            buttonSelected,
            "Button in content window was selected via touchstart"
          );
          ok(
            !defaultPrevented,
            "Touchstart event's default actions should no longer be prevented"
          );
        }
      );

      try {
        info("Picture-in-Picture window should open");
        await BrowserTestUtils.waitForCondition(
          () => Services.wm.getEnumerator(WINDOW_TYPE).hasMoreElements(),
          "Found a Picture-in-Picture"
        );
        for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
          if (!win.closed) {
            ok(true, "Found a Picture-in-Picture window as expected");
            win.close();
          }
        }
      } catch {
        ok(false, "No Picture-in-Picture window found, which is unexpected");
      }
    }
  );
});
