/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.autoScroll", true],
      ["middlemouse.paste", true],
      ["middlemouse.contentLoadURL", false],
      ["test.events.async.enabled", false],
    ],
  });

  let autoScroller;
  function onPopupShown(aEvent) {
    if (aEvent.originalTarget.id != "autoscroller") {
      return false;
    }
    autoScroller = aEvent.originalTarget;
    return true;
  }
  window.addEventListener("popupshown", onPopupShown, { capture: true });
  registerCleanupFunction(() => {
    window.removeEventListener("popupshown", onPopupShown, { capture: true });
  });
  function popupIsNotClosed() {
    return autoScroller && autoScroller.state != "closed";
  }

  async function promiseNativeMouseMiddleButtonDown(aBrowser) {
    await EventUtils.promiseNativeMouseEvent({
      type: "mousemove",
      target: aBrowser,
      atCenter: true,
    });
    return EventUtils.promiseNativeMouseEvent({
      type: "mousedown",
      target: aBrowser,
      atCenter: true,
      button: 1, // middle button
    });
  }
  async function promiseNativeMouseMiddleButtonUp(aBrowser) {
    return EventUtils.promiseNativeMouseEvent({
      type: "mouseup",
      target: aBrowser,
      atCenter: true,
      button: 1, // middle button
    });
  }
  function promiseWaitForAutoScrollerClosed() {
    if (!autoScroller || autoScroller.state == "closed") {
      return Promise.resolve();
    }
    let result = BrowserTestUtils.waitForEvent(
      autoScroller,
      "popuphidden",
      { capture: true },
      () => {
        return true;
      }
    );
    EventUtils.synthesizeKey("KEY_Escape");
    return result;
  }

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<div contenteditable style="height: 10000px;"></div>';
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await TestUtils.waitForCondition(
          popupIsNotClosed,
          "Wait for timeout of popup",
          100,
          10
        );
        ok(
          false,
          "Autoscroll shouldn't be started on editable <div> if middle paste is enabled"
        );
      } catch (e) {
        ok(
          typeof e == "string" && e.includes(" - timed out after 10 tries."),
          `Autoscroll shouldn't be started on editable <div> if middle paste is enabled (${
            typeof e == "string" ? e : e.message
          })`
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<div style="height: 10000px;"></div>';
        content.document.designMode = "on";
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await TestUtils.waitForCondition(
          popupIsNotClosed,
          "Wait for timeout of popup",
          100,
          10
        );
        ok(
          false,
          "Autoscroll shouldn't be started in document whose designMode is 'on' if middle paste is enabled"
        );
      } catch (e) {
        ok(
          typeof e == "string" && e.includes(" - timed out after 10 tries."),
          `Autoscroll shouldn't be started in document whose designMode is 'on' if middle paste is enabled (${
            typeof e == "string" ? e : e.message
          })`
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<div contenteditable style="height: 10000px;"><div contenteditable="false" style="height: 10000px;"></div></div>';
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await BrowserTestUtils.waitForEvent(
          window,
          "popupshown",
          { capture: true },
          onPopupShown
        );
        ok(
          true,
          "Auto scroll should be started on non-editable <div> in an editing host if middle paste is enabled"
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );

  await SpecialPowers.pushPrefEnv({
    set: [["middlemouse.paste", false]],
  });

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<div contenteditable style="height: 10000px;"></div>';
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await BrowserTestUtils.waitForEvent(
          window,
          "popupshown",
          { capture: true },
          onPopupShown
        );
        ok(
          true,
          "Auto scroll should be started on editable <div> if middle paste is disabled"
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<div style="height: 10000px;"></div>';
        content.document.designMode = "on";
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await BrowserTestUtils.waitForEvent(
          window,
          "popupshown",
          { capture: true },
          onPopupShown
        );
        ok(
          true,
          "Auto scroll should be started in document whose designMode is 'on' if middle paste is disabled"
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );

  await SpecialPowers.pushPrefEnv({
    set: [["middlemouse.paste", false]],
  });

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<input style="height: 10000px; width: 10000px;">';
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await BrowserTestUtils.waitForEvent(
          window,
          "popupshown",
          { capture: true },
          onPopupShown
        );
        ok(
          true,
          "Auto scroll should be started on <input> if middle paste is disabled"
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );

  await SpecialPowers.pushPrefEnv({
    set: [["middlemouse.paste", true]],
  });

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<input style="height: 10000px; width: 10000px;">';
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
      });
      await promiseNativeMouseMiddleButtonDown(browser);
      try {
        await TestUtils.waitForCondition(
          popupIsNotClosed,
          "Wait for timeout of popup",
          100,
          10
        );
        ok(
          false,
          "Autoscroll shouldn't be started on <input> if middle paste is enabled"
        );
      } catch (e) {
        ok(
          typeof e == "string" && e.includes(" - timed out after 10 tries."),
          `Autoscroll shouldn't be started on <input> if middle paste is enabled (${
            typeof e == "string" ? e : e.message
          })`
        );
      } finally {
        await promiseNativeMouseMiddleButtonUp(browser);
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        await waitForAutoScrollEnd;
      }
    }
  );
});
