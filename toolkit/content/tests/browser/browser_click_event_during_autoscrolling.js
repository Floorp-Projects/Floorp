/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.autoScroll", true],
      ["middlemouse.contentLoadURL", false],
      ["test.events.async.enabled", false],
    ],
  });

  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function(browser) {
      ok(browser.isRemoteBrowser, "This test passes only in e10s mode");
      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML =
          '<div style="height: 10000px;"></div>';
        content.document.documentElement.scrollTop = 500;
        content.document.documentElement.scrollTop; // Flush layout.
        // Prevent to open context menu when testing the secondary button click.
        content.window.addEventListener(
          "contextmenu",
          event => event.preventDefault(),
          { capture: true }
        );
      });

      function promiseFlushLayoutInContent() {
        return SpecialPowers.spawn(browser, [], () => {
          content.document.documentElement.scrollTop; // Flush layout in the remote content.
        });
      }

      function promiseContentTick() {
        return SpecialPowers.spawn(browser, [], async () => {
          await new Promise(r => {
            content.requestAnimationFrame(() => {
              content.requestAnimationFrame(r);
            });
          });
        });
      }

      let autoScroller;
      function promiseWaitForAutoScrollerOpen() {
        if (autoScroller?.state == "open") {
          info("The autoscroller has already been open");
          return Promise.resolve();
        }
        return BrowserTestUtils.waitForEvent(
          window,
          "popupshown",
          { capture: true },
          event => {
            if (event.originalTarget.id != "autoscroller") {
              return false;
            }
            autoScroller = event.originalTarget;
            info('"popupshown" event is fired');
            autoScroller.getBoundingClientRect(); // Flush layout of the autoscroller
            return true;
          }
        );
      }

      function promiseWaitForAutoScrollerClosed() {
        if (!autoScroller || autoScroller.state == "closed") {
          info("The autoscroller has already been closed");
          return Promise.resolve();
        }
        return BrowserTestUtils.waitForEvent(
          autoScroller,
          "popuphidden",
          { capture: true },
          () => {
            info('"popuphidden" event is fired');
            return true;
          }
        );
      }

      // Unfortunately, we cannot use synthesized mouse events for starting and
      // stopping autoscrolling because they may run different path from user
      // operation especially when there is a popup.

      /**
       * Instead of using `waitForContentEvent`, we use `addContentEventListener`
       * for checking which events are fired because `waitForContentEvent` cannot
       * detect redundant event since it's removed automatically at first event
       * or timeout if the expected count is 0.
       */
      class ContentEventCounter {
        constructor(aBrowser, aEventTypes) {
          this.eventData = new Map();
          for (let eventType of aEventTypes) {
            const removeEventListener = BrowserTestUtils.addContentEventListener(
              aBrowser,
              eventType,
              () => {
                let eventData = this.eventData.get(eventType);
                eventData.count++;
              },
              { capture: true }
            );
            this.eventData.set(eventType, {
              count: 0, // how many times the event fired.
              removeEventListener, // function to remove the event listener.
            });
          }
        }

        getCountAndRemoveEventListener(aEventType) {
          let eventData = this.eventData.get(aEventType);
          if (eventData.removeEventListener) {
            eventData.removeEventListener();
            eventData.removeEventListener = null;
          }
          return eventData.count;
        }

        promiseMouseEvents(aEventTypes, aMessage) {
          let needsToWait = [];
          for (const eventType of aEventTypes) {
            let eventData = this.eventData.get(eventType);
            if (eventData.count > 0) {
              info(`${aMessage}: Waiting "${eventType}" event in content...`);
              needsToWait.push(
                // Let's use `waitForCondition` here.  "timeout" is not worthwhile
                // to debug this test.  We want clearer failure log.
                TestUtils.waitForCondition(
                  () => eventData.count > 0,
                  `${aMessage}: "${eventType}" should be fired, but timed-out`
                )
              );
              break;
            }
          }
          return Promise.all(needsToWait);
        }
      }

      await (async function testMouseEventsAtStartingAutoScrolling() {
        info(
          "Waiting autoscroller popup for testing mouse events at starting autoscrolling"
        );
        await promiseFlushLayoutInContent();
        let eventsInContent = new ContentEventCounter(browser, [
          "click",
          "auxclick",
          "mousedown",
          "mouseup",
          "paste",
        ]);
        // Ensure that the event listeners added in the content with accessing
        // the remote content.
        await promiseFlushLayoutInContent();
        await EventUtils.promiseNativeMouseEvent({
          type: "mousemove",
          target: browser,
          atCenter: true,
        });
        const waitForOpenAutoScroll = promiseWaitForAutoScrollerOpen();
        await EventUtils.promiseNativeMouseEvent({
          type: "mousedown",
          target: browser,
          atCenter: true,
          button: 1, // middle button
        });
        await waitForOpenAutoScroll;
        // In the wild, native "mouseup" event occurs after the popup is open.
        await EventUtils.promiseNativeMouseEvent({
          type: "mouseup",
          target: browser,
          atCenter: true,
          button: 1, // middle button
        });
        await promiseFlushLayoutInContent();
        await promiseContentTick();
        await eventsInContent.promiseMouseEvents(
          ["mouseup"],
          "At starting autoscrolling"
        );
        for (let eventType of ["click", "auxclick", "paste"]) {
          is(
            eventsInContent.getCountAndRemoveEventListener(eventType),
            0,
            `"${eventType}" event shouldn't be fired in the content when a middle click starts autoscrolling`
          );
        }
        for (let eventType of ["mousedown", "mouseup"]) {
          is(
            eventsInContent.getCountAndRemoveEventListener(eventType),
            1,
            `"${eventType}" event should be fired in the content when a middle click starts autoscrolling`
          );
        }
        info("Waiting autoscroller close for preparing the following tests");
        let waitForAutoScrollEnd = promiseWaitForAutoScrollerClosed();
        EventUtils.synthesizeKey("KEY_Escape");
        await waitForAutoScrollEnd;
      })();

      async function doTestMouseEventsAtStoppingAutoScrolling({
        aButton = 0,
        aClickOutsideAutoScroller = false,
        aDescription = "Unspecified",
      }) {
        info(
          `Starting autoscrolling for testing to stop autoscrolling with ${aDescription}`
        );
        await promiseFlushLayoutInContent();
        await EventUtils.promiseNativeMouseEvent({
          type: "mousemove",
          target: browser,
          atCenter: true,
        });
        const waitForOpenAutoScroll = promiseWaitForAutoScrollerOpen();
        await EventUtils.promiseNativeMouseEvent({
          type: "mousedown",
          target: browser,
          atCenter: true,
          button: 1, // middle button
        });
        // In the wild, native "mouseup" event occurs after the popup is open.
        await waitForOpenAutoScroll;
        await EventUtils.promiseNativeMouseEvent({
          type: "mouseup",
          target: browser,
          atCenter: true,
          button: 1, // middle button
        });
        await promiseFlushLayoutInContent();
        // Just to be sure, wait for a tick for wait APZ stable.
        await TestUtils.waitForTick();

        let eventsInContent = new ContentEventCounter(browser, [
          "click",
          "auxclick",
          "mousedown",
          "mouseup",
          "paste",
          "contextmenu",
        ]);
        // Ensure that the event listeners added in the content with accessing
        // the remote content.
        await promiseFlushLayoutInContent();

        aDescription = `Stop autoscrolling with ${aDescription}`;
        info(
          `${aDescription}: Synthesizing primary mouse button event on the autoscroller`
        );
        const autoScrollerRect = autoScroller.getOuterScreenRect();
        info(
          `${aDescription}: autoScroller: { left: ${autoScrollerRect.left}, top: ${autoScrollerRect.top}, width: ${autoScrollerRect.width}, height: ${autoScrollerRect.height} }`
        );
        const waitForCloseAutoScroller = promiseWaitForAutoScrollerClosed();
        if (aClickOutsideAutoScroller) {
          info(
            `${aDescription}: Synthesizing mousemove move cursor outside the autoscroller...`
          );
          await EventUtils.promiseNativeMouseEvent({
            type: "mousemove",
            target: autoScroller,
            offsetX: -10,
            offsetY: -10,
            elementOnWidget: browser, // use widget for the parent window of the autoscroller
          });
          info(
            `${aDescription}: Synthesizing mousedown to stop autoscrolling...`
          );
          await EventUtils.promiseNativeMouseEvent({
            type: "mousedown",
            target: autoScroller,
            offsetX: -10,
            offsetY: -10,
            button: aButton,
            elementOnWidget: browser, // use widget for the parent window of the autoscroller
          });
        } else {
          info(
            `${aDescription}: Synthesizing mousemove move cursor onto the autoscroller...`
          );
          await EventUtils.promiseNativeMouseEvent({
            type: "mousemove",
            target: autoScroller,
            atCenter: true,
            elementOnWidget: browser, // use widget for the parent window of the autoscroller
          });
          info(
            `${aDescription}: Synthesizing mousedown to stop autoscrolling...`
          );
          await EventUtils.promiseNativeMouseEvent({
            type: "mousedown",
            target: autoScroller,
            atCenter: true,
            button: aButton,
            elementOnWidget: browser, // use widget for the parent window of the autoscroller
          });
        }
        // In the wild, native "mouseup" event occurs after the popup is closed.
        await waitForCloseAutoScroller;
        info(
          `${aDescription}: Synthesizing mouseup event for preceding mousedown which is for stopping autoscrolling`
        );
        await EventUtils.promiseNativeMouseEvent({
          type: "mouseup",
          target: browser,
          atCenter: true,
          button: aButton,
        });
        await promiseFlushLayoutInContent();
        await promiseContentTick();
        await eventsInContent.promiseMouseEvents(
          aButton != 2 ? ["mouseup"] : ["mouseup", "contextmenu"],
          aDescription
        );
        is(
          autoScroller.state,
          "closed",
          `${aDescription}: The autoscroller should've been closed`
        );
        // - On macOS, when clicking outside autoscroller, nsChildView
        // intentionally blocks both "mousedown" and "mouseup" events in the
        // case of the primary button click, and only "mousedown" for the
        // middle button when the "mousedown".  I'm not sure how it should work
        // on macOS for conforming to the platform manner.  Note that autoscroll
        // isn't available on the other browsers on macOS.  So, there is no
        // reference, but for consistency between platforms, it may be better
        // to ignore the platform manner.
        // - On Windows, when clicking outside autoscroller, nsWindow
        // intentionally blocks only "mousedown" events for the primary button
        // and the middle button.  But this behavior is different from Chrome
        // so that we need to fix this in the future.
        // - On Linux, when clicking outside autoscroller, nsWindow
        // intentionally blocks only "mousedown" events for any buttons.  But
        // on Linux, autoscroll isn't available by the default settings.  So,
        // not so urgent, but should be fixed in the future for consistency
        // between platforms and compatibility with Chrome on Windows.
        const rollingUpPopupConsumeMouseDown =
          aClickOutsideAutoScroller &&
          (aButton != 2 || navigator.platform.includes("Linux"));
        const rollingUpPopupConsumeMouseUp =
          aClickOutsideAutoScroller &&
          aButton == 0 &&
          navigator.platform.includes("Mac");
        const checkFuncForClick =
          aClickOutsideAutoScroller &&
          aButton == 2 &&
          !navigator.platform.includes("Linux")
            ? todo_is
            : is;
        for (let eventType of ["click", "auxclick"]) {
          checkFuncForClick(
            eventsInContent.getCountAndRemoveEventListener(eventType),
            0,
            `${aDescription}: "${eventType}" event shouldn't be fired in the remote content`
          );
        }
        is(
          eventsInContent.getCountAndRemoveEventListener("paste"),
          0,
          `${aDescription}: "paste" event shouldn't be fired in the remote content`
        );
        const checkFuncForMouseDown = rollingUpPopupConsumeMouseDown
          ? todo_is
          : is;
        checkFuncForMouseDown(
          eventsInContent.getCountAndRemoveEventListener("mousedown"),
          1,
          `${aDescription}: "mousedown" event should be fired in the remote content`
        );
        const checkFuncForMouseUp = rollingUpPopupConsumeMouseUp ? todo_is : is;
        checkFuncForMouseUp(
          eventsInContent.getCountAndRemoveEventListener("mouseup"),
          1,
          `${aDescription}: "mouseup" event should be fired in the remote content`
        );
        const checkFuncForContextMenu =
          aButton == 2 &&
          aClickOutsideAutoScroller &&
          navigator.platform.includes("Linux")
            ? todo_is
            : is;
        checkFuncForContextMenu(
          eventsInContent.getCountAndRemoveEventListener("contextmenu"),
          aButton == 2 ? 1 : 0,
          `${aDescription}: "contextmenu" event should${
            aButton != 2 ? " not" : ""
          } be fired in the remote content`
        );

        const promiseClickEvent = BrowserTestUtils.waitForContentEvent(
          browser,
          "click",
          {
            capture: true,
          }
        );
        await promiseFlushLayoutInContent();
        info(`${aDescription}: Waiting for click event in the remote content`);
        EventUtils.synthesizeNativeMouseEvent({
          type: "click",
          target: browser,
          atCenter: true,
        });
        await promiseClickEvent;
        ok(
          true,
          `${aDescription}: click event is fired in the remote content after stopping autoscrolling`
        );
      }

      // Clicking the primary button to stop autoscrolling.
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 0,
        aClickOutsideAutoScroller: false,
        aDescription: "a primary button click on autoscroller",
      });
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 0,
        aClickOutsideAutoScroller: true,
        aDescription: "a primary button click outside autoscroller",
      });

      // Clicking the secondary button to stop autoscrolling.
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 2,
        aClickOutsideAutoScroller: false,
        aDescription: "a secondary button click on autoscroller",
      });
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 2,
        aClickOutsideAutoScroller: true,
        aDescription: "a secondary button click outside autoscroller",
      });

      // Clicking the middle button to stop autoscrolling.
      await SpecialPowers.pushPrefEnv({ set: [["middlemouse.paste", true]] });
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 1,
        aClickOutsideAutoScroller: false,
        aDescription:
          "a middle button click on autoscroller (middle click paste enabled)",
      });
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 1,
        aClickOutsideAutoScroller: true,
        aDescription:
          "a middle button click outside autoscroller (middle click paste enabled)",
      });
      await SpecialPowers.pushPrefEnv({ set: [["middlemouse.paste", false]] });
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 1,
        aClickOutsideAutoScroller: false,
        aDescription:
          "a middle button click on autoscroller (middle click paste disabled)",
      });
      await doTestMouseEventsAtStoppingAutoScrolling({
        aButton: 1,
        aClickOutsideAutoScroller: true,
        aDescription:
          "a middle button click outside autoscroller (middle click paste disabled)",
      });
    }
  );
});
