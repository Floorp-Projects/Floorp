/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

const EVENTUTILS_URL =
  "chrome://mochikit/content/tests/SimpleTest/EventUtils.js";
var EventUtils = {};

Services.scriptloader.loadSubScript(EVENTUTILS_URL, EventUtils);

async function detachTab(tab) {
  let newWindowPromise = new Promise((resolve, reject) => {
    let observe = (win, topic, data) => {
      Services.obs.removeObserver(observe, "domwindowopened");
      resolve(win);
    };
    Services.obs.addObserver(observe, "domwindowopened");
  });

  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: tab,

    // destElement is null because tab detaching happens due
    // to a drag'n'drop on an invalid drop target.
    destElement: null,

    // don't move horizontally because that could cause a tab move
    // animation, and there's code to prevent a tab detaching if
    // the dragged tab is released while the animation is running.
    stepX: 0,
    stepY: 100,
  });

  return newWindowPromise;
}

/**
 * Tests that tabs dragged between windows with PiP open, the pip attribute stays
 */
add_task(async function test_dragging_pip_to_other_window() {
  // initialize
  let win1 = await BrowserTestUtils.openNewBrowserWindow();
  let win2 = await BrowserTestUtils.openNewBrowserWindow();

  let pipTab = await BrowserTestUtils.openNewForegroundTab(
    win1.gBrowser,
    TEST_PAGE
  );
  let destTab = await BrowserTestUtils.openNewForegroundTab(win2.gBrowser);

  let awaitCloseEventPromise = BrowserTestUtils.waitForEvent(
    pipTab,
    "TabClose"
  );
  let tabSwapPictureInPictureEventPromise = BrowserTestUtils.waitForEvent(
    pipTab,
    "TabSwapPictureInPicture"
  );

  // Open PiP
  let videoID = "with-controls";
  let browser = pipTab.linkedBrowser;

  let pipWin = await triggerPictureInPicture(browser, videoID);
  ok(pipWin, "Got Picture-in-Picture window.");

  // tear out window
  let effect = EventUtils.synthesizeDrop(
    pipTab,
    destTab,
    [[{ type: TAB_DROP_TYPE, data: pipTab }]],
    null,
    win1,
    win2
  );
  is(effect, "move", "Tab should be moved from win1 to win2.");

  let closeEvent = await awaitCloseEventPromise;
  let swappedPipTabsEvent = await tabSwapPictureInPictureEventPromise;

  is(
    closeEvent.detail.adoptedBy,
    swappedPipTabsEvent.detail,
    "Pip tab adopted by new tab created when original tab closed"
  );

  // make sure we reassign the pip tab to the new one
  pipTab = swappedPipTabsEvent.detail;

  // check PiP attribute
  ok(pipTab.hasAttribute("pictureinpicture"), "Tab should have PiP attribute");

  // end PiP
  let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
  let closeButton = pipWin.document.getElementById("close");
  EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
  await pipClosed;

  // ensure PiP attribute is gone
  await TestUtils.waitForCondition(
    () => !pipTab.hasAttribute("pictureinpicture"),
    "pictureinpicture attribute was removed"
  );

  ok(true, "pictureinpicture attribute successfully cleared");

  // close windows
  await BrowserTestUtils.closeWindow(win1);
  await BrowserTestUtils.closeWindow(win2);
});

/**
 * Tests that tabs torn out into a new window with PiP open, the pip attribute stays
 */
add_task(async function test_dragging_pip_into_new_window() {
  // initialize
  await BrowserTestUtils.withNewTab(
    {
      url: TEST_PAGE,
      gBrowser,
    },
    async browser => {
      // Create PiP
      let videoID = "with-controls";
      let pipTab = gBrowser.getTabForBrowser(browser);

      let pipWin = await triggerPictureInPicture(browser, videoID);

      let tabSwapPictureInPictureEventPromise = BrowserTestUtils.waitForEvent(
        pipTab,
        "TabSwapPictureInPicture"
      );

      // tear out into new window
      let newWin = await detachTab(pipTab);

      let swappedPipTabsEvent = await tabSwapPictureInPictureEventPromise;
      pipTab = swappedPipTabsEvent.detail;

      // check PiP attribute
      ok(
        pipTab.hasAttribute("pictureinpicture"),
        "Tab should have PiP attribute"
      );

      // end PiP

      let pipClosed = BrowserTestUtils.domWindowClosed(pipWin);
      let closeButton = pipWin.document.getElementById("close");
      EventUtils.synthesizeMouseAtCenter(closeButton, {}, pipWin);
      await pipClosed;

      // ensure pip attribute is gone
      await TestUtils.waitForCondition(
        () => !pipTab.hasAttribute("pictureinpicture"),
        "pictureinpicture attribute was removed"
      );
      ok(true, "pictureinpicture attribute successfully cleared");

      // close windows
      await BrowserTestUtils.closeWindow(newWin);
    }
  );
});
