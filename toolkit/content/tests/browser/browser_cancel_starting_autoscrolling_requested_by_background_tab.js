/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function testStopStartingAutoScroll() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.autoScroll", true],
      ["middlemouse.contentLoadURL", false],
      ["test.events.async.enabled", true],
      [
        "accessibility.mouse_focuses_formcontrol",
        !navigator.platform.includes("Mac"),
      ],
    ],
  });

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      async function doTest({
        aInnerHTML,
        aDescription,
        aExpectedActiveElement,
      }) {
        await SpecialPowers.spawn(browser, [aInnerHTML], async contentHTML => {
          content.document.body.innerHTML = contentHTML;
          content.document.documentElement.scrollTop; // Flush layout.
          const iframe = content.document.querySelector("iframe");
          // If the test page has an iframe, we need to ensure it has loaded.
          if (!iframe || iframe.contentDocument?.readyState == "complete") {
            return;
          }
          // It's too late to check "load" event.  Let's check
          // Document#readyState instead.
          await ContentTaskUtils.waitForCondition(
            () => iframe.contentDocument?.readyState == "complete",
            "Waiting for loading the subdocument"
          );
        });

        let autoScroller;
        let onPopupShown = event => {
          if (event.originalTarget.id !== "autoscroller") {
            return false;
          }
          autoScroller = event.originalTarget;
          info(`${aDescription}: "popupshown" event is fired`);
          autoScroller.getBoundingClientRect(); // Flush layout of the autoscroller
          return true;
        };
        window.addEventListener("popupshown", onPopupShown, { capture: true });
        registerCleanupFunction(() => {
          window.removeEventListener("popupshown", onPopupShown, {
            capture: true,
          });
        });

        let waitForNewTabForeground = BrowserTestUtils.waitForEvent(
          gBrowser,
          "TabSwitchDone"
        );
        await EventUtils.promiseNativeMouseEvent({
          type: "mousedown",
          button: 1, // middle click
          target: browser,
          atCenter: true,
        });
        info(`${aDescription}: Waiting for active tab switched...`);
        await waitForNewTabForeground;
        // To confirm that autoscrolling won't start accidentally, we should
        // wait a while.
        // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
        await new Promise(resolve => setTimeout(resolve, 500));
        is(
          autoScroller,
          undefined,
          `${aDescription}: autoscroller should not be open because requested tab is now in background`
        );
        // Clean up
        await EventUtils.promiseNativeMouseEvent({
          type: "mouseup",
          button: 1, // middle click
          target: browser,
          atCenter: true,
        }); // release implicit capture
        EventUtils.synthesizeKey("KEY_Escape"); // To close unexpected autoscroller
        isnot(
          browser,
          gBrowser.selectedBrowser,
          `${aDescription}: The clicked tab shouldn't be foreground tab`
        );
        is(
          gBrowser.selectedTab,
          gBrowser.tabs[gBrowser.tabs.length - 1],
          `${aDescription}: The opened tab should be foreground tab`
        );
        await SpecialPowers.spawn(
          browser,
          [aExpectedActiveElement, aDescription],
          (expectedActiveElement, description) => {
            if (expectedActiveElement != null) {
              if (expectedActiveElement == "iframe") {
                // Check only whether the subdocument gets focus.
                return;
              }
              Assert.equal(
                content.document.activeElement,
                content.document.querySelector(expectedActiveElement),
                `${description}: Active element should be the result of querySelector("${expectedActiveElement}")`
              );
            } else {
              Assert.deepEqual(
                content.document.activeElement,
                new content.window.Object(null),
                `${description}: No element should be active`
              );
            }
          }
        );
        gBrowser.removeTab(gBrowser.tabs[gBrowser.tabs.length - 1]);
        await SimpleTest.promiseFocus(browser);
        if (aExpectedActiveElement == "iframe") {
          await SpecialPowers.spawn(browser, [aDescription], description => {
            // XXX Due to no `Assert#todo`, this checks opposite result.
            Assert.ok(
              !content.document
                .querySelector("iframe")
                .contentDocument.hasFocus(),
              `TODO: ${description}: The subdocument should have focus when the tab gets foreground`
            );
          });
        }
      }
      await doTest({
        aInnerHTML: `<div style="height: 10000px;" onmousedown="window.open('https://example.com/', '_blank')">Click to open new tab</div>`,
        aDescription: "Clicking non-focusable <div> with middle mouse button",
        aExpectedActiveElement: null,
      });
      await doTest({
        aInnerHTML: `<button style="height: 10000px; width: 100%" onmousedown="window.open('https://example.com/', '_blank')">Click to open new tab</button>`,
        aDescription: `Clicking focusable <button> with middle mouse button`,
        aExpectedActiveElement: navigator.platform.includes("Mac")
          ? null
          : "button",
      });
      await doTest({
        aInnerHTML: `<iframe style="height: 90vh; width: 90vw" srcdoc="<div onmousedown='window.open(\`https://example.com/\`, \`_blank\`)' style='width: 100%; height: 10000px'>Click to open new tab"></iframe>`,
        aDescription: `Clicking non-focusable <div> in <iframe> with middle mouse button`,
        aExpectedActiveElement: "iframe",
      });
    }
  );
});
