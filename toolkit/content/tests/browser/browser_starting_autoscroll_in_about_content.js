/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testStartingAutoScrollInAboutContent() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.autoScroll", true],
      ["middlemouse.contentLoadURL", false],
      ["test.events.async.enabled", true],
    ],
  });

  await BrowserTestUtils.withNewTab("about:support", async function (browser) {
    let autoScroller;
    let promiseStartAutoScroll = new Promise(resolve => {
      let onPopupShown = event => {
        if (event.originalTarget.id != "autoscroller") {
          return;
        }
        autoScroller = event.originalTarget;
        info('"popupshown" event is fired');
        autoScroller.getBoundingClientRect(); // Flush layout of the autoscroller
        resolve();
      };
      window.addEventListener("popupshown", onPopupShown, { capture: true });
      registerCleanupFunction(() => {
        window.removeEventListener("popupshown", onPopupShown, {
          capture: true,
        });
      });
    });

    ok(!browser.isRemoteBrowser, "Browser should not be remote.");
    await ContentTask.spawn(browser, null, async function () {
      await ContentTaskUtils.waitForCondition(
        () =>
          content.document.documentElement.scrollHeight >
          content.document.documentElement.clientHeight,
        "The document should become scrollable"
      );
    });

    await EventUtils.promiseNativeMouseEvent({
      type: "mousemove",
      target: browser,
      offsetX: 10,
      offsetY: 10, // XXX Assuming that there is no interactive content here.
    });
    await EventUtils.promiseNativeMouseEvent({
      type: "mousedown",
      button: 1, // middle click
      target: browser,
      offsetX: 10,
      offsetY: 10,
    });
    info("Waiting to start autoscrolling");
    await promiseStartAutoScroll;
    ok(autoScroller != null, "Autoscrolling should be started");
    await EventUtils.promiseNativeMouseEvent({
      type: "mouseup",
      button: 1, // middle click
      target: browser,
      offsetX: 10,
      offsetY: 10,
    }); // release implicit capture
    EventUtils.synthesizeKey("KEY_Escape"); // Close autoscroller
    await TestUtils.waitForCondition(
      () => autoScroller.state == "closed",
      "autoscroll should be canceled"
    );
  });
});
