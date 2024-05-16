/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      let NativeKeyConstants = {};
      Services.scriptloader.loadSubScript(
        "chrome://mochikit/content/tests/SimpleTest/NativeKeyCodes.js",
        NativeKeyConstants
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.document.body.innerHTML = "<input>";
        content.document.querySelector("input").focus();
      });

      function promiseAltKeyUp() {
        return new Promise(resolve =>
          addEventListener(
            "keyup",
            function onKeyUp(event) {
              if (event.key == "Alt") {
                removeEventListener("keyup", onKeyUp, { capture: true });
                requestAnimationFrame(resolve);
              }
            },
            { capture: true }
          )
        );
      }

      /**
       * This assumes that Alt + X does not match with any mnemonic.  If UI is
       * changed, change the character to unused one.
       *
       * @param {boolean} aWithChars    true if WM_SYSCHAR should be synthesized.
       * @returns {Promise}             Promise resolved when the window gets a
       *                                keyup event.
       */
      function promiseSynthesizeAltX(aWithChars) {
        const waitForAltKeyUp = promiseAltKeyUp();
        EventUtils.synthesizeNativeKey(
          EventUtils.KEYBOARD_LAYOUT_EN_US,
          NativeKeyConstants.WIN_VK_X,
          { altKey: true },
          aWithChars ? "x" : "",
          "x"
        );
        return waitForAltKeyUp;
      }

      function promiseSynthesizeAltArrowUp() {
        const waitForAltKeyUp = promiseAltKeyUp();
        EventUtils.synthesizeNativeKey(
          EventUtils.KEYBOARD_LAYOUT_EN_US,
          NativeKeyConstants.WIN_VK_UP,
          { altKey: true },
          "",
          ""
        );
        return waitForAltKeyUp;
      }

      function stringifyEvents(aEvents) {
        if (!aEvents.length) {
          return "[]";
        }
        function stringifyEvent(aEvent) {
          return `{ type: "${aEvent.type}", key: "${aEvent.key}", defaultPrevented: ${aEvent.defaultPrevented}}`;
        }
        let result = "";
        for (const event of aEvents) {
          if (result == "") {
            result = "[ ";
          } else {
            result += ", ";
          }
          result += stringifyEvent(event);
        }
        return result + " ]";
      }

      let events = [];
      function onKeyDownOrKeyPress(aEvent) {
        if (aEvent.key == "Alt") {
          return;
        }
        events.push({
          type: aEvent.type,
          key: aEvent.key,
          defaultPrevented: aEvent.defaultPrevented,
        });
      }
      browser.addEventListener("keydown", onKeyDownOrKeyPress);
      browser.addEventListener("keypress", onKeyDownOrKeyPress);

      // If Alt + F is fired with WM_SYSKEYDOWN and WM_SYSCHAR, the keydown and
      // keypress events should be fired.
      events = [];
      await promiseSynthesizeAltX(true);
      is(
        stringifyEvents(events),
        stringifyEvents([
          { type: "keydown", key: "x", defaultPrevented: false },
          { type: "keypress", key: "x", defaultPrevented: false },
        ]),
        "Alt + X with WM_SYSCHAR message should cause keydown and keypress"
      );

      // If Alt + F is fired only with WM_SYSKEYDOWN the keydown should be
      // consumed and keypress events should not be fired.
      events = [];
      await promiseSynthesizeAltX(false);
      is(
        stringifyEvents(events),
        stringifyEvents([
          { type: "keydown", key: "x", defaultPrevented: true },
        ]),
        "Alt + X without WM_SYSCHAR message should cause only consumed keydown"
      );

      // If Alt + <function key> is fired only with WM_SYSKEYDOWN, then, the
      // keydown should not be consumed and keypress events should be fired.
      events = [];
      await promiseSynthesizeAltArrowUp();
      is(
        stringifyEvents(events),
        stringifyEvents([
          { type: "keydown", key: "ArrowUp", defaultPrevented: false },
          { type: "keypress", key: "ArrowUp", defaultPrevented: false },
        ]),
        "Alt + ArrowUp without WM_SYSCHAR message should cause keydown and keypress"
      );

      browser.removeEventListener("keydown", onKeyDownOrKeyPress);
      browser.removeEventListener("keypress", onKeyDownOrKeyPress);
    }
  );
});
