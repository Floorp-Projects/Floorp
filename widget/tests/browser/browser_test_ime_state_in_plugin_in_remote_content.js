/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../file_ime_state_test_helper.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/widget/tests/browser/file_ime_state_test_helper.js",
  this
);
add_task(async function () {
  await BrowserTestUtils.withNewTab(
    "https://example.com/browser/toolkit/content/tests/browser/file_empty.html",
    async function (browser) {
      const tipWrapper = new TIPWrapper(window);
      ok(
        tipWrapper.isAvailable(),
        "TextInputProcessor should've been initialized"
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.wrappedJSObject.waitForIMEContentObserverSendingNotifications =
          () => {
            return new content.window.Promise(resolve =>
              content.window.requestAnimationFrame(() =>
                content.window.requestAnimationFrame(resolve)
              )
            );
          };
        content.document.body.innerHTML =
          '<input><object type="application/x-test"></object>';
      });

      await SpecialPowers.spawn(browser, [], () => {
        content.document.activeElement?.blur();
        return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
      });
      is(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        "IME enabled state should be disabled when no element has focus"
      );
      ok(
        !tipWrapper.IMEHasFocus,
        "IME should not have focus when no element has focus"
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("object").focus();
        return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
      });
      is(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        "IME enabled state should be disabled when an <object> for plugin has focus"
      );
      ok(
        !tipWrapper.IMEHasFocus,
        "IME enabled state should not have focus when an <object> for plugin has focus"
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("object").blur();
        return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
      });
      is(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        "IME enabled state should be disabled when an <object> for plugin gets blurred"
      );
      ok(
        !tipWrapper.IMEHasFocus,
        "IME should not have focus when an <object> for plugin gets blurred"
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("object").focus();
        return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
      });
      is(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        "IME enabled state should be disabled when an <object> for plugin gets focused again"
      );
      ok(
        !tipWrapper.IMEHasFocus,
        "IME should not have focus when an <object> for plugin gets focused again"
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("object").remove();
        return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
      });
      is(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
        "IME enabled state should be disabled when focused <object> for plugin is removed from the document"
      );
      ok(
        !tipWrapper.IMEHasFocus,
        "IME should not have focus when focused <object> for plugin is removed from the document"
      );

      await SpecialPowers.spawn(browser, [], () => {
        content.document.querySelector("input").focus();
        return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
      });
      is(
        window.windowUtils.IMEStatus,
        Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
        "IME enabled state should be enabled after <input> gets focus"
      );
      ok(
        tipWrapper.IMEHasFocus,
        "IME should have focus after <input> gets focus"
      );
    }
  );
});
