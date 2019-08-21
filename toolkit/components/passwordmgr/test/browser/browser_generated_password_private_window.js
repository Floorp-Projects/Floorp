// The origin for the test URIs.
const TEST_ORIGIN = "https://example.com";
const FORM_PAGE_PATH =
  "/browser/toolkit/components/passwordmgr/test/browser/form_basic.html";
const passwordInputSelector = "#form-basic-password";

add_task(async function test_autocomplete_popup_item_hidden() {
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
      let popup = doc.getElementById("PopupAutoComplete");
      ok(popup, "Got popup");
      await openACPopup(popup, browser, passwordInputSelector);

      let item = popup.querySelector(`[originaltype="generatedPassword"]`);
      ok(!item, "Should not get 'Generate password' richlistitem");

      let onPopupClosed = BrowserTestUtils.waitForCondition(
        () => !popup.popupOpen,
        "Popup should get closed"
      );

      await TestUtils.waitForTick();
      popup.closePopup();
      await onPopupClosed;
    }
  );
  await BrowserTestUtils.closeWindow(win);
});

add_task(async function test_autocomplete_menu_item_disabled() {
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
        generatedPasswordItem.getAttribute("disabled"),
        "true",
        "Generate password context menu item should be disabled in PB mode"
      );
      document.getElementById("contentAreaContextMenu").hidePopup();
    }
  );
  await BrowserTestUtils.closeWindow(win);
});
