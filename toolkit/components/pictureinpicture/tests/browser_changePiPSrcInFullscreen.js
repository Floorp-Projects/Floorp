/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const NEW_VIDEO_ASPECT_RATIO = 1.334;

async function switchVideoSource(browser, src) {
  await ContentTask.spawn(browser, { src }, async ({ src }) => {
    let doc = content.document;
    let video = doc.getElementById("no-controls");
    video.src = src;
  });
}

/**
 *
 * @param {Object} actual The actual size and position of the window
 * @param {Object} expected The expected size and position of the window
 * @param {String} message A message to print before asserting the size and position
 */
function assertEvent(actual, expected, message) {
  info(message);
  isfuzzy(
    actual.width,
    expected.width,
    ACCEPTABLE_DIFFERENCE,
    `The actual width: ${actual.width}. The expected width: ${expected.width}`
  );
  isfuzzy(
    actual.height,
    expected.height,
    ACCEPTABLE_DIFFERENCE,
    `The actual height: ${actual.height}. The expected height: ${expected.height}`
  );
  isfuzzy(
    actual.left,
    expected.left,
    ACCEPTABLE_DIFFERENCE,
    `The actual left: ${actual.left}. The expected left: ${expected.left}`
  );
  isfuzzy(
    actual.top,
    expected.top,
    ACCEPTABLE_DIFFERENCE,
    `The actual top: ${actual.top}. The expected top: ${expected.top}`
  );
}

/**
 * This test is our control test. This tests that when the PiP window exits
 * fullscreen it will return to the size it was before being fullscreened.
 */
add_task(async function testNoSrcChangeFullscreen() {
  // After opening the PiP window, it is resized to 640x360. There is a change
  // the PiP window will open with that size. To prevent that we override the
  // last saved position so we open at (0, 0) and 300x300.
  overrideSavedPosition(0, 0, 300, 300);
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);
      let pipWin = await triggerPictureInPicture(browser, "no-controls");
      let controls = pipWin.document.getElementById("controls");
      const screen = pipWin.screen;

      let resizeEventArray = [];
      pipWin.addEventListener("resize", event => {
        let win = event.target;
        let obj = {
          width: win.innerWidth,
          height: win.innerHeight,
          left: win.screenLeft,
          top: win.screenTop,
        };
        resizeEventArray.push(obj);
      });

      // Move the PiP window to an unsaved location
      let left = 100;
      let top = 100;
      pipWin.moveTo(left, top);

      await BrowserTestUtils.waitForCondition(
        () => pipWin.screenLeft === 100 && pipWin.screenTop === 100,
        "Waiting for PiP to move to 100, 100"
      );

      let width = 640;
      let height = 360;

      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      pipWin.resizeTo(width, height);
      await resizePromise;

      Assert.equal(
        resizeEventArray.length,
        1,
        "resizeEventArray should have 1 event"
      );

      let actualEvent = resizeEventArray.splice(0, 1)[0];
      let expectedEvent = { width, height, left, top };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly positioned before fullscreen"
      );

      await promiseFullscreenEntered(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to enter fullscreen."
      );

      await BrowserTestUtils.waitForCondition(
        () => resizeEventArray.length === 1,
        "Waiting for resizeEventArray to have 1 event"
      );

      actualEvent = resizeEventArray.splice(0, 1)[0];
      expectedEvent = {
        width: screen.width,
        height: screen.height,
        left: screen.left,
        top: screen.top,
      };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly fullscreened before switching source"
      );

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Double-click caused us to exit fullscreen."
      );

      await BrowserTestUtils.waitForCondition(
        () => resizeEventArray.length >= 1,
        "Waiting for resizeEventArray to have 1 event, got " +
          resizeEventArray.length
      );

      actualEvent = resizeEventArray.splice(0, 1)[0];
      expectedEvent = { width, height, left, top };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly positioned after exiting fullscreen"
      );

      await ensureMessageAndClosePiP(browser, "no-controls", pipWin, false);

      clearSavedPosition();
    }
  );
});

/**
 * This function tests changing the src of a Picture-in-Picture player while
 * the player is fullscreened and then ensuring the that video stays
 * fullscreened after the src change and that the player will resize to the new
 * video size.
 */
add_task(async function testChangingSameSizeVideoSrcFullscreen() {
  // After opening the PiP window, it is resized to 640x360. There is a change
  // the PiP window will open with that size. To prevent that we override the
  // last saved position so we open at (0, 0) and 300x300.
  overrideSavedPosition(0, 0, 300, 300);
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);
      let pipWin = await triggerPictureInPicture(browser, "no-controls");
      let controls = pipWin.document.getElementById("controls");
      const screen = pipWin.screen;
      let sandbox = sinon.createSandbox();
      let resizeToVideoSpy = sandbox.spy(pipWin, "resizeToVideo");

      let resizeEventArray = [];
      pipWin.addEventListener("resize", event => {
        let win = event.target;
        let obj = {
          width: win.innerWidth,
          height: win.innerHeight,
          left: win.screenLeft,
          top: win.screenTop,
        };
        resizeEventArray.push(obj);
      });

      // Move the PiP window to an unsaved location
      let left = 100;
      let top = 100;
      pipWin.moveTo(left, top);

      await BrowserTestUtils.waitForCondition(
        () => pipWin.screenLeft === 100 && pipWin.screenTop === 100,
        "Waiting for PiP to move to 100, 100"
      );

      let width = 640;
      let height = 360;

      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      pipWin.resizeTo(width, height);
      await resizePromise;

      Assert.equal(
        resizeEventArray.length,
        1,
        "resizeEventArray should have 1 event"
      );

      let actualEvent = resizeEventArray.splice(0, 1)[0];
      let expectedEvent = { width, height, left, top };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly positioned before fullscreen"
      );

      await promiseFullscreenEntered(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to enter fullscreen."
      );

      await BrowserTestUtils.waitForCondition(
        () => resizeEventArray.length === 1,
        "Waiting for resizeEventArray to have 1 event"
      );

      actualEvent = resizeEventArray.splice(0, 1)[0];
      expectedEvent = {
        width: screen.width,
        height: screen.height,
        left: screen.left,
        top: screen.top,
      };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly fullscreened before switching source"
      );

      await switchVideoSource(browser, "test-video.mp4");

      await BrowserTestUtils.waitForCondition(
        () => resizeToVideoSpy.calledOnce,
        "Waiting for deferredResize to be updated"
      );

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Double-click caused us to exit fullscreen."
      );

      await BrowserTestUtils.waitForCondition(
        () => resizeEventArray.length >= 1,
        "Waiting for resizeEventArray to have 1 event, got " +
          resizeEventArray.length
      );

      actualEvent = resizeEventArray.splice(0, 1)[0];
      expectedEvent = { width, height, left, top };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly positioned after exiting fullscreen"
      );

      sandbox.restore();
      await ensureMessageAndClosePiP(browser, "no-controls", pipWin, false);

      clearSavedPosition();
    }
  );
});

/**
 * This is similar to the previous test but in this test we switch to a video
 * with a different aspect ratio to confirm that the PiP window will take the
 * new aspect ratio after exiting fullscreen. We also exit fullscreen with the
 * escape key instead of double clicking in this test.
 */
add_task(async function testChangingDifferentSizeVideoSrcFullscreen() {
  // After opening the PiP window, it is resized to 640x360. There is a change
  // the PiP window will open with that size. To prevent that we override the
  // last saved position so we open at (0, 0) and 300x300.
  overrideSavedPosition(0, 0, 300, 300);
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      await ensureVideosReady(browser);
      let pipWin = await triggerPictureInPicture(browser, "no-controls");
      let controls = pipWin.document.getElementById("controls");
      const screen = pipWin.screen;
      let sandbox = sinon.createSandbox();
      let resizeToVideoSpy = sandbox.spy(pipWin, "resizeToVideo");

      let resizeEventArray = [];
      pipWin.addEventListener("resize", event => {
        let win = event.target;
        let obj = {
          width: win.innerWidth,
          height: win.innerHeight,
          left: win.screenLeft,
          top: win.screenTop,
        };
        resizeEventArray.push(obj);
      });

      // Move the PiP window to an unsaved location
      let left = 100;
      let top = 100;
      pipWin.moveTo(left, top);

      await BrowserTestUtils.waitForCondition(
        () => pipWin.screenLeft === 100 && pipWin.screenTop === 100,
        "Waiting for PiP to move to 100, 100"
      );

      let width = 640;
      let height = 360;

      let resizePromise = BrowserTestUtils.waitForEvent(pipWin, "resize");
      pipWin.resizeTo(width, height);
      await resizePromise;

      Assert.equal(
        resizeEventArray.length,
        1,
        "resizeEventArray should have 1 event"
      );

      let actualEvent = resizeEventArray.splice(0, 1)[0];
      let expectedEvent = { width, height, left, top };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly positioned before fullscreen"
      );

      await promiseFullscreenEntered(pipWin, async () => {
        EventUtils.sendMouseEvent(
          {
            type: "dblclick",
          },
          controls,
          pipWin
        );
      });

      Assert.equal(
        pipWin.document.fullscreenElement,
        pipWin.document.body,
        "Double-click caused us to enter fullscreen."
      );

      await BrowserTestUtils.waitForCondition(
        () => resizeEventArray.length === 1,
        "Waiting for resizeEventArray to have 1 event"
      );

      actualEvent = resizeEventArray.splice(0, 1)[0];
      expectedEvent = {
        width: screen.width,
        height: screen.height,
        left: screen.left,
        top: screen.top,
      };
      assertEvent(
        actualEvent,
        expectedEvent,
        "The PiP window has been correctly fullscreened before switching source"
      );

      let previousWidth = pipWin.getDeferredResize().width;

      await switchVideoSource(browser, "test-video-long.mp4");

      // Confirm that we are updating the `deferredResize` and not actually resizing
      await BrowserTestUtils.waitForCondition(
        () => resizeToVideoSpy.calledOnce,
        "Waiting for deferredResize to be updated"
      );

      // Confirm that we updated the deferredResize to the new width
      await BrowserTestUtils.waitForCondition(
        () => previousWidth !== pipWin.getDeferredResize().width,
        "Waiting for deferredResize to update"
      );

      await promiseFullscreenExited(pipWin, async () => {
        EventUtils.synthesizeKey("KEY_Escape", {}, pipWin);
      });

      Assert.ok(
        !pipWin.document.fullscreenElement,
        "Escape key caused us to exit fullscreen."
      );

      await BrowserTestUtils.waitForCondition(
        () => resizeEventArray.length >= 1,
        "Waiting for resizeEventArray to have 1 event, got " +
          resizeEventArray.length
      );

      actualEvent = resizeEventArray.splice(0, 1)[0];
      expectedEvent = {
        width: height * NEW_VIDEO_ASPECT_RATIO,
        height,
        left,
        top,
      };

      // When two resize events happen very close together we optimize by
      // "coalescing" the two resizes into a single resize event. Sometimes
      // the events aren't "coalesced" together (I don't know why) so I check
      // if the most recent event is what we are looking for and if it is not
      // then I'll wait for the resize event we are looking for.
      if (
        Math.abs(
          actualEvent.width - expectedEvent.width <= ACCEPTABLE_DIFFERENCE
        )
      ) {
        // The exit fullscreen resize events were "coalesced".
        assertEvent(
          actualEvent,
          expectedEvent,
          "The PiP window has been correctly positioned after exiting fullscreen"
        );
      } else {
        // For some reason the exit fullscreen resize events weren't "coalesced"
        // so we have to wait for the next resize event.
        await BrowserTestUtils.waitForCondition(
          () => resizeEventArray.length === 1,
          "Waiting for resizeEventArray to have 1 event"
        );

        actualEvent = resizeEventArray.splice(0, 1)[0];

        assertEvent(
          actualEvent,
          expectedEvent,
          "The PiP window has been correctly positioned after exiting fullscreen"
        );
      }

      sandbox.restore();
      await ensureMessageAndClosePiP(browser, "no-controls", pipWin, false);

      clearSavedPosition();
    }
  );
});
