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
        content.wrappedJSObject.resetIMEStateWithFocusMove = () => {
          const input = content.document.createElement("input");
          content.document.body.appendChild(input);
          input.focus();
          input.remove();
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        };
        content.document.body.innerHTML = "<div></div>";
      });

      function resetIMEStateWithFocusMove() {
        return SpecialPowers.spawn(browser, [], () => {
          const input = content.document.createElement("input");
          content.document.body.appendChild(input);
          input.focus();
          input.remove();
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
      }

      await (async function test_setting_contenteditable_of_focused_div() {
        await SpecialPowers.spawn(browser, [], () => {
          const div = content.document.querySelector("div");
          div.setAttribute("tabindex", "0");
          div.focus();
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          "test_setting_contenteditable_of_focused_div: IME should be disabled when non-editable <div> has focus"
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .querySelector("div")
            .setAttribute("contenteditable", "");
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
          "test_setting_contenteditable_of_focused_div: IME should be enabled when contenteditable of focused <div> is set"
        );
        ok(
          tipWrapper.IMEHasFocus,
          "test_setting_contenteditable_of_focused_div: IME should have focus when contenteditable of focused <div> is set"
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document
            .querySelector("div")
            .removeAttribute("contenteditable");
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          "test_setting_contenteditable_of_focused_div: IME should be disabled when contenteditable of focused <div> is removed"
        );
        ok(
          !tipWrapper.IMEHasFocus,
          "test_setting_contenteditable_of_focused_div: IME should not have focus when contenteditable of focused <div> is removed"
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.querySelector("div").removeAttribute("tabindex");
        });
      })();

      await resetIMEStateWithFocusMove();

      await (async function test_removing_contenteditable_of_non_last_editable_div() {
        await SpecialPowers.spawn(browser, [], async () => {
          const div = content.document.querySelector("div");
          div.setAttribute("tabindex", "0");
          div.setAttribute("contenteditable", "");
          const anotherEditableDiv = content.document.createElement("div");
          anotherEditableDiv.setAttribute("contenteditable", "");
          div.parentElement.appendChild(anotherEditableDiv);
          div.focus();
          await content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
          div.removeAttribute("contenteditable");
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          "test_removing_contenteditable_of_non_last_editable_div: IME should be disabled when contenteditable of focused <div> is removed"
        );
        ok(
          !tipWrapper.IMEHasFocus,
          "test_removing_contenteditable_of_non_last_editable_div: IME should not have focus when contenteditable of focused <div> is removed"
        );
        await SpecialPowers.spawn(browser, [], () => {
          const divs = content.document.querySelectorAll("div");
          divs[1].remove();
          divs[0].removeAttribute("tabindex");
        });
      })();

      await resetIMEStateWithFocusMove();

      await (async function test_setting_designMode() {
        await SpecialPowers.spawn(browser, [], () => {
          content.window.focus();
          content.document.designMode = "on";
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
          'test_setting_designMode: IME should be enabled when designMode is set to "on"'
        );
        ok(
          tipWrapper.IMEHasFocus,
          'test_setting_designMode: IME should have focus when designMode is set to "on"'
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.designMode = "off";
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          'test_setting_designMode: IME should be disabled when designMode is set to "off"'
        );
        ok(
          !tipWrapper.IMEHasFocus,
          'test_setting_designMode: IME should not have focus when designMode is set to "off"'
        );
      })();

      await resetIMEStateWithFocusMove();

      async function test_setting_content_editable_of_body_when_shadow_DOM_has_focus(
        aMode
      ) {
        await SpecialPowers.spawn(browser, [aMode], mode => {
          const div = content.document.querySelector("div");
          const shadow = div.attachShadow({ mode });
          content.wrappedJSObject.divInShadow =
            content.document.createElement("div");
          content.wrappedJSObject.divInShadow.setAttribute("tabindex", "0");
          shadow.appendChild(content.wrappedJSObject.divInShadow);
          content.wrappedJSObject.divInShadow.focus();
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          `test_setting_content_editable_of_body_when_shadow_DOM_has_focus(${aMode}): IME should be disabled when non-editable <div> in a shadow DOM has focus`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.body.setAttribute("contenteditable", "");
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        // todo_is because of bug 1807597.  Gecko does not update focus when focused
        // element becomes an editable child.  Therefore, cannot initialize
        // HTMLEditor with the new editing host.
        todo_is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
          `test_setting_content_editable_of_body_when_shadow_DOM_has_focus(${aMode}): IME should be enabled when the <body> becomes editable`
        );
        todo(
          tipWrapper.IMEHasFocus,
          `test_setting_content_editable_of_body_when_shadow_DOM_has_focus(${aMode}): IME should have focus when the <body> becomes editable`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.body.removeAttribute("contenteditable");
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          `test_setting_content_editable_of_body_when_shadow_DOM_has_focus)${aMode}): IME should be disabled when the <body> becomes not editable`
        );
        ok(
          !tipWrapper.IMEHasFocus,
          `test_setting_content_editable_of_body_when_shadow_DOM_has_focus)${aMode}): IME should not have focus when the <body> becomes not editable`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.querySelector("div").remove();
          content.document.body.appendChild(
            content.document.createElement("div")
          );
        });
      }

      async function test_setting_designMode_when_shadow_DOM_has_focus(aMode) {
        await SpecialPowers.spawn(browser, [aMode], mode => {
          const div = content.document.querySelector("div");
          const shadow = div.attachShadow({ mode });
          content.wrappedJSObject.divInShadow =
            content.document.createElement("div");
          content.wrappedJSObject.divInShadow.setAttribute("tabindex", "0");
          shadow.appendChild(content.wrappedJSObject.divInShadow);
          content.wrappedJSObject.divInShadow.focus();
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should be disabled when non-editable <div> in a shadow DOM has focus`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.designMode = "on";
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should stay disabled when designMode is set`
        );
        ok(
          !tipWrapper.IMEHasFocus,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should not have focus when designMode is set`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.wrappedJSObject.divInShadow.setAttribute(
            "contenteditable",
            ""
          );
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        // todo_is because of bug 1807597.  Gecko does not update focus when focused
        // document is into the design mode.  Therefore, cannot initialize
        // HTMLEditor with the document node properly.
        todo_is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_ENABLED,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should be enabled when focused <div> in a shadow DOM becomes editable`
        );
        todo(
          tipWrapper.IMEHasFocus,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should have focus when focused <div> in a shadow DOM becomes editable`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.designMode = "off";
          return content.wrappedJSObject.waitForIMEContentObserverSendingNotifications();
        });
        is(
          window.windowUtils.IMEStatus,
          Ci.nsIDOMWindowUtils.IME_STATUS_DISABLED,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should be disabled when designMode is unset`
        );
        ok(
          !tipWrapper.IMEHasFocus,
          `test_setting_designMode_when_shadow_DOM_has_focus(${aMode}): IME should not have focus when designMode is unset`
        );
        await SpecialPowers.spawn(browser, [], () => {
          content.document.querySelector("div").remove();
          content.document.body.appendChild(
            content.document.createElement("div")
          );
        });
      }

      for (const mode of ["open", "closed"]) {
        await test_setting_content_editable_of_body_when_shadow_DOM_has_focus(
          mode
        );
        await resetIMEStateWithFocusMove();
        await test_setting_designMode_when_shadow_DOM_has_focus(mode);
        await resetIMEStateWithFocusMove();
      }
    }
  );
});
