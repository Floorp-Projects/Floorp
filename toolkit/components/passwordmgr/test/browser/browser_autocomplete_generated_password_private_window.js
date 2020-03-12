// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const FORM_PAGE_PATH =
  "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const passwordInputSelector = "#form-basic-password";

add_task(async function setup() {
  Services.telemetry.clearEvents();
  TelemetryTestUtils.assertEvents([], {
    category: "pwmgr",
    method: "autocomplete_shown",
  });
});

add_task(async function test_autocomplete_new_password_popup_item_visible() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: TEST_ORIGIN,
    },
    async function(browser) {
      info("Generate and cache a password for a non-private context");
      let lmp = browser.browsingContext.currentWindowGlobal.getActor(
        "LoginManager"
      );
      lmp.getGeneratedPassword();
      is(
        LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().size,
        1,
        "Non-Private password should be cached"
      );
    }
  );

  await LoginTestUtils.addLogin({ username: "username", password: "pass1" });
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  const doc = win.document;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: win.gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser.ownerGlobal);
      await ContentTask.spawn(
        browser,
        [passwordInputSelector],
        function openAutocomplete(sel) {
          content.document.querySelector(sel).autocomplete = "new-password";
        }
      );

      let popup = doc.getElementById("PopupAutoComplete");
      ok(popup, "Got popup");
      await openACPopup(popup, browser, passwordInputSelector);

      let item = popup.querySelector(`[originaltype="generatedPassword"]`);
      ok(item, "Should get 'Generate password' richlistitem");

      let onPopupClosed = BrowserTestUtils.waitForCondition(
        () => !popup.popupOpen,
        "Popup should get closed"
      );

      await TestUtils.waitForTick();

      TelemetryTestUtils.assertEvents(
        [["pwmgr", "autocomplete_shown", "generatedpassword"]],
        { category: "pwmgr", method: "autocomplete_shown" }
      );

      await closePopup(popup);
      await onPopupClosed;
    }
  );

  let lastPBContextExitedPromise = TestUtils.topicObserved(
    "last-pb-context-exited"
  ).then(() => TestUtils.waitForTick());
  await BrowserTestUtils.closeWindow(win);
  await lastPBContextExitedPromise;
  is(
    LoginManagerParent.getGeneratedPasswordsByPrincipalOrigin().size,
    1,
    "Only private-context passwords should be cleared"
  );
});

add_task(async function test_autocomplete_menu_item_enabled() {
  const win = await BrowserTestUtils.openNewBrowserWindow({ private: true });
  const doc = win.document;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser: win.gBrowser,
      url: TEST_ORIGIN + FORM_PAGE_PATH,
    },
    async function(browser) {
      await SimpleTest.promiseFocus(browser);
      await openPasswordContextMenu(browser, passwordInputSelector);
      let generatedPasswordItem = doc.getElementById(
        "fill-login-generated-password"
      );
      is(
        generatedPasswordItem.disabled,
        false,
        "Generate password context menu item should be enabled in PB mode"
      );
      await closePopup(document.getElementById("contentAreaContextMenu"));
    }
  );
  await BrowserTestUtils.closeWindow(win);
});
