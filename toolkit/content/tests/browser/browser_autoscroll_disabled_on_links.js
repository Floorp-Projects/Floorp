/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_autoscroll_links() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.autoScroll", true],
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

  async function testMarkup(markup) {
    return BrowserTestUtils.withNewTab(
      "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
      async function (browser) {
        await SpecialPowers.spawn(browser, [markup], html => {
          content.document.body.innerHTML = html;
          content.document.documentElement.scrollTop = 1;
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
          ok(false, "Autoscroll shouldn't be started on " + markup);
        } catch (e) {
          ok(
            typeof e == "string" && e.includes(" - timed out after 10 tries."),
            `Autoscroll shouldn't be started on ${markup} (${
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
  }

  await testMarkup(
    '<a href="https://example.com/" style="display: block; position: absolute; height:100%; width:100%; background: aqua">Click me</a>'
  );

  await testMarkup(`
    <svg viewbox="0 0 100 100" style="display: block; height: 100%; width: 100%;">
      <a href="https://example.com/">
        <rect height=100 width=100 fill=blue />
      </a>
    </svg>`);

  await testMarkup(`
    <a href="https://example.com/">
      <svg viewbox="0 0 100 100" style="display: block; height: 100%; width: 100%;">
        <use href="#x"/>
      </svg>
    </a>

    <svg viewbox="0 0 100 100" style="display: none">
      <rect id="x" height=100 width=100 fill=green />
    </svg>
    `);
});
