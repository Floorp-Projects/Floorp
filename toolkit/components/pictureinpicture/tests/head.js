/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TOGGLE_POLICIES } = ChromeUtils.import(
  "resource://gre/modules/PictureInPictureControls.jsm"
);

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const TEST_ROOT_2 = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.org"
);
const TEST_PAGE = TEST_ROOT + "test-page.html";
const TEST_PAGE_2 = TEST_ROOT_2 + "test-page.html";
const TEST_PAGE_WITH_IFRAME = TEST_ROOT_2 + "test-page-with-iframe.html";
const TEST_PAGE_WITH_SOUND = TEST_ROOT + "test-page-with-sound.html";
const TEST_PAGE_WITH_NAN_VIDEO_DURATION =
  TEST_ROOT + "test-page-with-nan-video-duration.html";
const WINDOW_TYPE = "Toolkit:PictureInPicture";
const TOGGLE_POSITION_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.position";
const HAS_USED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.has-used";
const SHARED_DATA_KEY = "PictureInPicture:SiteOverrides";

/**
 * We currently ship with a few different variations of the
 * Picture-in-Picture toggle. The tests for Picture-in-Picture include tests
 * that check the style rules of various parts of the toggle. Since each toggle
 * variation has different style rules, we introduce a structure here to
 * describe the appearance of the toggle at different stages for the tests.
 *
 * The top-level structure looks like this:
 *
 * {
 *   rootID (String): The ID of the root element of the toggle.
 *   stages (Object): An Object representing the styles of the toggle at
 *     different stages of its use. Each property represents a different
 *     stage that can be tested. Right now, those stages are:
 *
 *     hoverVideo:
 *       When the mouse is hovering the video but not the toggle.
 *
 *     hoverToggle:
 *       When the mouse is hovering both the video and the toggle.
 *
 *       Both stages must be assigned an Object with the following properties:
 *
 *       opacities:
 *         This should be set to an Object where the key is a CSS selector for
 *         an element, and the value is a double for what the eventual opacity
 *         of that element should be set to.
 *
 *       hidden:
 *         This should be set to an Array of CSS selector strings for elements
 *         that should be hidden during a particular stage.
 * }
 *
 * DEFAULT_TOGGLE_STYLES is the set of styles for the default variation of the
 * toggle.
 */
const DEFAULT_TOGGLE_STYLES = {
  rootID: "pictureInPictureToggle",
  stages: {
    hoverVideo: {
      opacities: {
        ".pip-wrapper": 0.8,
      },
      hidden: [".pip-expanded"],
    },

    hoverToggle: {
      opacities: {
        ".pip-wrapper": 1.0,
      },
      hidden: [".pip-expanded"],
    },
  },
};

/**
 * Given a browser and the ID for a <video> element, triggers
 * Picture-in-Picture for that <video>, and resolves with the
 * Picture-in-Picture window once it is ready to be used.
 *
 * @param {Element,BrowsingContext} browser The <xul:browser> or
 * BrowsingContext hosting the <video>
 *
 * @param {String} videoID The ID of the video to trigger
 * Picture-in-Picture on.
 *
 * @return Promise
 * @resolves With the Picture-in-Picture window when ready.
 */
async function triggerPictureInPicture(browser, videoID) {
  let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
  let videoReady = SpecialPowers.spawn(browser, [videoID], async videoID => {
    let video = content.document.getElementById(videoID);
    let event = new content.CustomEvent("MozTogglePictureInPicture", {
      bubbles: true,
    });
    video.dispatchEvent(event);
    await ContentTaskUtils.waitForCondition(() => {
      return video.isCloningElementVisually;
    }, "Video is being cloned visually.");
  });
  let win = await domWindowOpened;
  await Promise.all([
    SimpleTest.promiseFocus(win),
    win.promiseDocumentFlushed(() => {}),
    videoReady,
  ]);
  return win;
}

/**
 * Given a browser and the ID for a <video> element, checks that the
 * video is showing the "This video is playing in Picture-in-Picture mode."
 * status message overlay.
 *
 * @param {Element,BrowsingContext} browser The <xul:browser> or
 * BrowsingContext hosting the <video>
 *
 * @param {String} videoID The ID of the video to trigger
 * Picture-in-Picture on.
 *
 * @param {bool} expected True if we expect the message to be showing.
 *
 * @return Promise
 * @resolves When the checks have completed.
 */
async function assertShowingMessage(browser, videoID, expected) {
  let showing = await SpecialPowers.spawn(browser, [videoID], async videoID => {
    let video = content.document.getElementById(videoID);
    let shadowRoot = video.openOrClosedShadowRoot;
    let pipOverlay = shadowRoot.querySelector(".pictureInPictureOverlay");
    Assert.ok(pipOverlay, "Should be able to find Picture-in-Picture overlay.");

    let rect = pipOverlay.getBoundingClientRect();
    return rect.height > 0 && rect.width > 0;
  });
  Assert.equal(
    showing,
    expected,
    "Video should be showing the expected state."
  );
}

/**
 * Tests if a video is currently being cloned for a given content browser. Provides a
 * good indicator for answering if this video is currently open in PiP.
 *
 * @param {Browser} browser
 *   The content browser that the video lives in
 * @param {string} videoId
 *   The id associated with the video
 *
 * @returns {bool}
 *   Whether the video is currently being cloned (And is most likely open in PiP)
 */
function assertVideoIsBeingCloned(browser, videoId) {
  return SpecialPowers.spawn(browser, [videoId], async videoID => {
    let video = content.document.getElementById(videoID);
    await ContentTaskUtils.waitForCondition(() => {
      return video.isCloningElementVisually;
    }, "Video is being cloned visually.");
  });
}

/**
 * Ensures that each of the videos loaded inside of a document in a
 * <browser> have reached the HAVE_ENOUGH_DATA readyState.
 *
 * @param {Element} browser The <xul:browser> hosting the <video>(s)
 *
 * @return Promise
 * @resolves When each <video> is in the HAVE_ENOUGH_DATA readyState.
 */
async function ensureVideosReady(browser) {
  // PictureInPictureToggleChild waits for videos to fire their "canplay"
  // event before considering them for the toggle, so we start by making
  // sure each <video> has done this.
  info(`Waiting for videos to be ready`);
  await SpecialPowers.spawn(browser, [], async () => {
    let videos = this.content.document.querySelectorAll("video");
    for (let video of videos) {
      if (video.readyState < content.HTMLMediaElement.HAVE_ENOUGH_DATA) {
        info(`Waiting for 'canplaythrough' for '${video.id}'`);
        await ContentTaskUtils.waitForEvent(video, "canplaythrough");
      }
    }
  });
}

/**
 * Tests that the toggle opacity reaches or exceeds a certain threshold within
 * a reasonable time.
 *
 * @param {Element} browser The <xul:browser> that has the <video> in it.
 * @param {String} videoID The ID of the video element that we expect the toggle
 * to appear on.
 * @param {String} stage The stage for which the opacity is going to change. This
 * should be one of "hoverVideo" or "hoverToggle".
 * @param {Object} toggleStyles Optional argument. See the documentation for the
 * DEFAULT_TOGGLE_STYLES object for a sense of what styleRules is expected to be.
 *
 * @return Promise
 * @resolves When the check has completed.
 */
async function toggleOpacityReachesThreshold(
  browser,
  videoID,
  stage,
  toggleStyles = DEFAULT_TOGGLE_STYLES
) {
  let togglePosition = Services.prefs.getStringPref(
    TOGGLE_POSITION_PREF,
    "right"
  );
  let hasUsed = Services.prefs.getBoolPref(HAS_USED_PREF, false);
  let toggleStylesForStage = toggleStyles.stages[stage];
  info(
    `Testing toggle for stage ${stage} ` +
      `in position ${togglePosition}, has used: ${hasUsed}`
  );

  let args = { videoID, toggleStylesForStage, togglePosition, hasUsed };
  await SpecialPowers.spawn(browser, [args], async args => {
    let { videoID, toggleStylesForStage } = args;

    let video = content.document.getElementById(videoID);
    let shadowRoot = video.openOrClosedShadowRoot;

    for (let hiddenElement of toggleStylesForStage.hidden) {
      let el = shadowRoot.querySelector(hiddenElement);
      ok(
        ContentTaskUtils.is_hidden(el),
        `Expected ${hiddenElement} to be hidden.`
      );
    }

    for (let opacityElement in toggleStylesForStage.opacities) {
      let opacityThreshold = toggleStylesForStage.opacities[opacityElement];
      let el = shadowRoot.querySelector(opacityElement);

      await ContentTaskUtils.waitForCondition(
        () => {
          let opacity = parseFloat(this.content.getComputedStyle(el).opacity);
          return opacity >= opacityThreshold;
        },
        `Toggle element ${opacityElement} should have eventually reached ` +
          `target opacity ${opacityThreshold}`,
        100,
        100
      );
    }

    ok(true, "Toggle reached target opacity.");
  });
}

/**
 * Tests that the toggle has the correct policy attribute set. This should be called
 * either when the toggle is visible, or events have been queued such that the toggle
 * will soon be visible.
 *
 * @param {Element} browser The <xul:browser> that has the <video> in it.
 * @param {String} videoID The ID of the video element that we expect the toggle
 * to appear on.
 * @param {Number} policy Optional argument. If policy is defined, then it should
 * be one of the values in the TOGGLE_POLICIES from PictureInPictureControls.jsm.
 * If undefined, this function will ensure no policy attribute is set.
 *
 * @return Promise
 * @resolves When the check has completed.
 */
async function assertTogglePolicy(
  browser,
  videoID,
  policy,
  toggleStyles = DEFAULT_TOGGLE_STYLES
) {
  let toggleID = toggleStyles.rootID;
  let args = { videoID, toggleID, policy };
  await SpecialPowers.spawn(browser, [args], async args => {
    let { videoID, toggleID, policy } = args;

    let video = content.document.getElementById(videoID);
    let shadowRoot = video.openOrClosedShadowRoot;
    let controlsOverlay = shadowRoot.querySelector(".controlsOverlay");
    let toggle = shadowRoot.getElementById(toggleID);

    await ContentTaskUtils.waitForCondition(() => {
      return controlsOverlay.classList.contains("hovering");
    }, "Waiting for the hovering state to be set on the video.");

    if (policy) {
      const { TOGGLE_POLICY_STRINGS } = ChromeUtils.import(
        "resource://gre/modules/PictureInPictureControls.jsm"
      );
      let policyAttr = toggle.getAttribute("policy");
      Assert.equal(
        policyAttr,
        TOGGLE_POLICY_STRINGS[policy],
        "The correct toggle policy is set."
      );
    } else {
      Assert.ok(
        !toggle.hasAttribute("policy"),
        "No toggle policy should be set."
      );
    }
  });
}

/**
 * Tests that either all or none of the expected mousebutton events
 * fire in web content when clicking on the page.
 *
 * Note: This function will only work on pages that load the
 * click-event-helper.js script.
 *
 * @param {Element} browser The <xul:browser> that will receive the mouse
 * events.
 * @param {bool} isExpectingEvents True if we expect all of the normal
 * mouse button events to fire. False if we expect none of them to fire.
 * @param {bool} isExpectingClick True if the mouse events should include the
 * "click" event, which is only included when the primary mouse button is pressed.
 * @return Promise
 * @resolves When the check has completed.
 */
async function assertSawMouseEvents(
  browser,
  isExpectingEvents,
  isExpectingClick = true
) {
  const MOUSE_BUTTON_EVENTS = [
    "pointerdown",
    "mousedown",
    "pointerup",
    "mouseup",
  ];

  if (isExpectingClick) {
    MOUSE_BUTTON_EVENTS.push("click");
  }

  let mouseEvents = await SpecialPowers.spawn(browser, [], async () => {
    return this.content.wrappedJSObject.getRecordedEvents();
  });

  let expectedEvents = isExpectingEvents ? MOUSE_BUTTON_EVENTS : [];
  Assert.deepEqual(
    mouseEvents,
    expectedEvents,
    "Expected to get the right mouse events."
  );
}

/**
 * Ensures that a <video> inside of a <browser> is scrolled into view,
 * and then returns the coordinates of its Picture-in-Picture toggle as well
 * as whether or not the <video> element is showing the built-in controls.
 *
 * @param {Element} browser The <xul:browser> that has the <video> loaded in it.
 * @param {String} videoID The ID of the video that has the toggle.
 *
 * @return Promise
 * @resolves With the following Object structure:
 *   {
 *     controls: <Boolean>,
 *   }
 *
 * Where controls represents whether or not the video has the default control set
 * displayed.
 */
async function prepareForToggleClick(browser, videoID) {
  // Synthesize a mouse move just outside of the video to ensure that
  // the video is in a non-hovering state. We'll go 5 pixels to the
  // left and above the top-left corner.
  await BrowserTestUtils.synthesizeMouse(
    `#${videoID}`,
    -5,
    -5,
    {
      type: "mousemove",
    },
    browser,
    false
  );

  // For each video, make sure it's scrolled into view, and get the rect for
  // the toggle while we're at it.
  let args = { videoID };
  return SpecialPowers.spawn(browser, [args], async args => {
    let { videoID } = args;

    let video = content.document.getElementById(videoID);
    video.scrollIntoView({ behaviour: "instant" });

    if (!video.controls) {
      // For no-controls <video> elements, an IntersectionObserver is used
      // to know when we the PictureInPictureChild should begin tracking
      // mousemove events. We don't exactly know when that IntersectionObserver
      // will fire, so we poll a special testing function that will tell us when
      // the video that we care about is being tracked.
      let { PictureInPictureToggleChild } = ChromeUtils.import(
        "resource://gre/actors/PictureInPictureChild.jsm"
      );
      await ContentTaskUtils.waitForCondition(
        () => {
          return PictureInPictureToggleChild.isTracking(video);
        },
        "Waiting for PictureInPictureToggleChild to be tracking the video.",
        100,
        100
      );
    }

    let shadowRoot = video.openOrClosedShadowRoot;
    let controlsOverlay = shadowRoot.querySelector(".controlsOverlay");
    await ContentTaskUtils.waitForCondition(
      () => {
        return !controlsOverlay.classList.contains("hovering");
      },
      "Waiting for the video to not be hovered.",
      100,
      100
    );

    return {
      controls: video.controls,
    };
  });
}

/**
 * Returns client rect info for the toggle if it's supposed to be visible
 * on hover. Otherwise, returns client rect info for the video with the
 * associated ID.
 *
 * @param {Element} browser The <xul:browser> that has the <video> loaded in it.
 * @param {String} videoID The ID of the video that has the toggle.
 *
 * @return Promise
 * @resolves With the following Object structure:
 *   {
 *     top: <Number>,
 *     left: <Number>,
 *     width: <Number>,
 *     height: <Number>,
 *   }
 */
async function getToggleClientRect(
  browser,
  videoID,
  toggleStyles = DEFAULT_TOGGLE_STYLES
) {
  let args = { videoID, toggleID: toggleStyles.rootID };
  return ContentTask.spawn(browser, args, async args => {
    const { Rect } = ChromeUtils.import("resource://gre/modules/Geometry.jsm");

    let { videoID, toggleID } = args;
    let video = content.document.getElementById(videoID);
    let shadowRoot = video.openOrClosedShadowRoot;
    let toggle = shadowRoot.getElementById(toggleID);
    let rect = Rect.fromRect(toggle.getBoundingClientRect());

    let clickableChildren = toggle.querySelectorAll(".clickable");
    for (let child of clickableChildren) {
      let childRect = Rect.fromRect(child.getBoundingClientRect());
      rect.expandToContain(childRect);
    }

    if (!rect.width && !rect.height) {
      rect = video.getBoundingClientRect();
    }

    return {
      top: rect.top,
      left: rect.left,
      width: rect.width,
      height: rect.height,
    };
  });
}

/**
 * Test helper for the Picture-in-Picture toggle. Loads a page, and then
 * tests the provided video elements for the toggle both appearing and
 * opening the Picture-in-Picture window in the expected cases.
 *
 * @param {String} testURL The URL of the page with the <video> elements.
 * @param {Object} expectations An object with the following schema:
 *   <video-element-id>: {
 *     canToggle: {Boolean}
 *     policy: {Number} (optional)
 *     styleRules: {Object} (optional)
 *   }
 * If canToggle is true, then it's expected that moving the mouse over the
 * video and then clicking in the toggle region should open a
 * Picture-in-Picture window. If canToggle is false, we expect that a click
 * in this region will not result in the window opening.
 *
 * If policy is defined, then it should be one of the values in the
 * TOGGLE_POLICIES from PictureInPictureControls.jsm.
 *
 * See the documentation for the DEFAULT_TOGGLE_STYLES object for a sense
 * of what styleRules is expected to be. If left undefined, styleRules will
 * default to DEFAULT_TOGGLE_STYLES.
 *
 * @param {async Function} prepFn An optional asynchronous function to run
 * before running the toggle test. The function is passed the opened
 * <xul:browser> as its only argument once the testURL has finished loading.
 *
 * @return Promise
 * @resolves When the test is complete and the tab with the loaded page is
 * removed.
 */
async function testToggle(testURL, expectations, prepFn = async () => {}) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: testURL,
    },
    async browser => {
      await prepFn(browser);
      await ensureVideosReady(browser);

      for (let [videoID, { canToggle, policy, toggleStyles }] of Object.entries(
        expectations
      )) {
        await SimpleTest.promiseFocus(browser);
        info(`Testing video with id: ${videoID}`);

        await testToggleHelper(
          browser,
          videoID,
          canToggle,
          policy,
          toggleStyles
        );
      }
    }
  );
}

/**
 * Test helper for the Picture-in-Picture toggle. Given a loaded page with some
 * videos on it, tests that the toggle behaves as expected when interacted
 * with by the mouse.
 *
 * @param {Element} browser The <xul:browser> that has the <video> loaded in it.
 * @param {String} videoID The ID of the video that has the toggle.
 * @param {Boolean} canToggle True if we expect the toggle to be visible and
 * clickable by the mouse for the associated video.
 * @param {Number} policy Optional argument. If policy is defined, then it should
 * be one of the values in the TOGGLE_POLICIES from PictureInPictureControls.jsm.
 * @param {Object} toggleStyles Optional argument. See the documentation for the
 * DEFAULT_TOGGLE_STYLES object for a sense of what styleRules is expected to be.
 *
 * @return Promise
 * @resolves When the check for the toggle is complete.
 */
async function testToggleHelper(
  browser,
  videoID,
  canToggle,
  policy,
  toggleStyles
) {
  let { controls } = await prepareForToggleClick(browser, videoID);

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

  info("Checking toggle policy");
  await assertTogglePolicy(browser, videoID, policy, toggleStyles);

  if (canToggle) {
    info("Waiting for toggle to become visible");
    await toggleOpacityReachesThreshold(
      browser,
      videoID,
      "hoverVideo",
      toggleStyles
    );
  }

  let toggleClientRect = await getToggleClientRect(
    browser,
    videoID,
    toggleStyles
  );

  info("Hovering the toggle rect now.");
  let toggleCenterX = toggleClientRect.left + toggleClientRect.width / 2;
  let toggleCenterY = toggleClientRect.top + toggleClientRect.height / 2;

  await BrowserTestUtils.synthesizeMouseAtPoint(
    toggleCenterX,
    toggleCenterY,
    {
      type: "mousemove",
    },
    browser
  );
  await BrowserTestUtils.synthesizeMouseAtPoint(
    toggleCenterX,
    toggleCenterY,
    {
      type: "mouseover",
    },
    browser
  );

  if (canToggle) {
    info("Waiting for toggle to reach full opacity");
    await toggleOpacityReachesThreshold(
      browser,
      videoID,
      "hoverToggle",
      toggleStyles
    );
  }

  // First, ensure that a non-primary mouse click is ignored.
  info("Right-clicking on toggle.");

  await BrowserTestUtils.synthesizeMouseAtPoint(
    toggleCenterX,
    toggleCenterY,
    { button: 2 },
    browser
  );

  // For videos without the built-in controls, we expect that all mouse events
  // should have fired - otherwise, the events are all suppressed. For videos
  // with controls, none of the events should be fired, as the controls overlay
  // absorbs them all.
  //
  // Note that the right-click does not result in a "click" event firing.
  await assertSawMouseEvents(browser, !controls, false);

  // The message to open the Picture-in-Picture window would normally be sent
  // immediately before this Promise resolved, so the window should have opened
  // by now if it was going to happen.
  for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
    if (!win.closed) {
      ok(false, "Found a Picture-in-Picture window unexpectedly.");
      return;
    }
  }

  ok(true, "No Picture-in-Picture window found.");

  // Okay, now test with the primary mouse button.

  if (canToggle) {
    info(
      "Clicking on toggle, and expecting a Picture-in-Picture window to open"
    );
    let domWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded(null);
    await BrowserTestUtils.synthesizeMouseAtPoint(
      toggleCenterX,
      toggleCenterY,
      {},
      browser
    );
    let win = await domWindowOpened;
    ok(win, "A Picture-in-Picture window opened.");

    await assertVideoIsBeingCloned(browser, videoID);

    await BrowserTestUtils.closeWindow(win);

    // Make sure that clicking on the toggle resulted in no mouse button events
    // being fired in content.
    await assertSawMouseEvents(browser, false);
  } else {
    info(
      "Clicking on toggle, and expecting no Picture-in-Picture window opens"
    );
    await BrowserTestUtils.synthesizeMouseAtPoint(
      toggleCenterX,
      toggleCenterY,
      {},
      browser
    );

    // If we aren't showing the toggle, we expect all mouse events to be seen.
    await assertSawMouseEvents(browser, !controls);

    // The message to open the Picture-in-Picture window would normally be sent
    // immediately before this Promise resolved, so the window should have opened
    // by now if it was going to happen.
    for (let win of Services.wm.getEnumerator(WINDOW_TYPE)) {
      if (!win.closed) {
        ok(false, "Found a Picture-in-Picture window unexpectedly.");
        return;
      }
    }

    ok(true, "No Picture-in-Picture window found.");
  }

  // Click on the very top-left pixel of the document and ensure that we
  // see all of the mouse events for it.
  await BrowserTestUtils.synthesizeMouseAtPoint(1, 1, {}, browser);
  await assertSawMouseEvents(browser, true);
}

/**
 * Helper function that ensures that a provided async function
 * causes a window to fully enter fullscreen mode.
 *
 * @param window (DOM Window)
 *   The window that is expected to enter fullscreen mode.
 * @param asyncFn (Async Function)
 *   The async function to run to trigger the fullscreen switch.
 * @return Promise
 * @resolves When the fullscreen entering transition completes.
 */
async function promiseFullscreenEntered(window, asyncFn) {
  let entered = BrowserTestUtils.waitForEvent(
    window,
    "MozDOMFullscreen:Entered"
  );

  await asyncFn();

  await entered;

  await BrowserTestUtils.waitForCondition(() => {
    return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
  });
}

/**
 * Helper function that ensures that a provided async function
 * causes a window to fully exit fullscreen mode.
 *
 * @param window (DOM Window)
 *   The window that is expected to exit fullscreen mode.
 * @param asyncFn (Async Function)
 *   The async function to run to trigger the fullscreen switch.
 * @return Promise
 * @resolves When the fullscreen exiting transition completes.
 */
async function promiseFullscreenExited(window, asyncFn) {
  let exited = BrowserTestUtils.waitForEvent(window, "MozDOMFullscreen:Exited");

  await asyncFn();

  await exited;

  await BrowserTestUtils.waitForCondition(() => {
    return !TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS");
  });

  if (AppConstants.platform == "macosx") {
    // On macOS, the fullscreen transition takes some extra time
    // to complete, and we don't receive events for it. We need to
    // wait for it to complete or else input events in the next test
    // might get eaten up. This is the best we can currently do.
    //
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    await new Promise(resolve => setTimeout(resolve, 2000));
  }
}

/**
 * Helper function that ensures that the "This video is
 * playing in Picture-in-Picture mode" message works,
 * then closes the player window
 *
 * @param {Element} browser The <xul:browser> that has the <video> loaded in it.
 * @param {String} videoID The ID of the video that has the toggle.
 * @param {Element} pipWin The Picture-in-Picture window that was opened
 * @param {Boolean} iframe True if the test is on an Iframe, which modifies
 * the test behavior
 */
async function ensureMessageAndClosePiP(browser, videoID, pipWin, isIframe) {
  try {
    await assertShowingMessage(browser, videoID, true);
  } finally {
    let uaWidgetUpdate = null;
    if (isIframe) {
      uaWidgetUpdate = SpecialPowers.spawn(browser, [], async () => {
        await ContentTaskUtils.waitForEvent(
          content.windowRoot,
          "UAWidgetSetupOrChange",
          true /* capture */
        );
      });
    } else {
      uaWidgetUpdate = BrowserTestUtils.waitForContentEvent(
        browser,
        "UAWidgetSetupOrChange",
        true /* capture */
      );
    }
    await BrowserTestUtils.closeWindow(pipWin);
    await uaWidgetUpdate;
  }
}

/**
 * Helper function that returns True if the specified video is paused
 * and False if the specified video is not paused.
 *
 * @param {Element} browser The <xul:browser> that has the <video> loaded in it.
 * @param {String} videoID The ID of the video to check.
 */
async function isVideoPaused(browser, videoID) {
  return SpecialPowers.spawn(browser, [videoID], async videoID => {
    return content.document.getElementById(videoID).paused;
  });
}
