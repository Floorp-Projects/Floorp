const { ChromeMigrationUtils } = ChromeUtils.import(
  "resource:///modules/ChromeMigrationUtils.jsm"
);
const { MigrationUtils } = ChromeUtils.import(
  "resource:///modules/MigrationUtils.jsm"
);
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

add_task(async function check_fluent_ids() {
  await document.l10n.ready;
  MozXULElement.insertFTLIfNeeded("toolkit/main-window/autocomplete.ftl");

  const host = "testhost.com";
  for (const browser of ChromeMigrationUtils.CONTEXTUAL_LOGIN_IMPORT_BROWSERS) {
    const id = `autocomplete-import-logins-${browser}`;
    const message = await document.l10n.formatValue(id, { host });
    ok(message.includes(`data-l10n-name="line1"`), `${id} included line1`);
    ok(message.includes(`data-l10n-name="line2"`), `${id} included line2`);
    ok(message.includes(host), `${id} replaced host`);
  }
});

// Showing importables updates counts delayed, so adjust and cleanup.
add_task(async function test_initialize() {
  const debounce = sinon
    .stub(LoginManagerParent, "SUGGEST_IMPORT_DEBOUNCE_MS")
    .value(0);
  const importable = sinon
    .stub(ChromeMigrationUtils, "getImportableLogins")
    .resolves(["chrome"]);

  // This makes the third autocomplete test *not* show import suggestions.
  Services.prefs.setIntPref("signon.suggestImportCount", 2);

  registerCleanupFunction(() => {
    debounce.restore();
    importable.restore();
    Services.prefs.clearUserPref("signon.suggestImportCount");
  });
});

add_task(async function import_suggestion_wizard() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function(browser) {
      const popup = document.getElementById("PopupAutoComplete");
      ok(popup, "Got popup");
      await openACPopup(popup, browser, "#form-basic-username");

      const importableItem = popup.querySelector(
        `[originaltype="importableLogins"]`
      );
      ok(importableItem, "Got importable suggestion richlistitem");

      await BrowserTestUtils.waitForCondition(
        () => !importableItem.collapsed,
        "Wait for importable suggestion to show"
      );

      info("Clicking on importable suggestion");
      const wizardPromise = BrowserTestUtils.waitForCondition(
        () => Services.wm.getMostRecentWindow("Browser:MigrationWizard"),
        "Wait for migration wizard to open"
      );

      // The modal window blocks execution, so avoid calling directly.
      executeSoon(() => EventUtils.synthesizeMouseAtCenter(importableItem, {}));

      const wizard = await wizardPromise;
      ok(wizard, "Wizard opened");

      await closePopup(popup);
      await BrowserTestUtils.closeWindow(wizard);
    }
  );
});

add_task(async function import_suggestion_learn_more() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function(browser) {
      const popup = document.getElementById("PopupAutoComplete");
      ok(popup, "Got popup");
      await openACPopup(popup, browser, "#form-basic-username");

      const learnMoreItem = popup.querySelector(`[type="importableLearnMore"]`);
      ok(learnMoreItem, "Got importable learn more richlistitem");

      await BrowserTestUtils.waitForCondition(
        () => !learnMoreItem.collapsed,
        "Wait for importable learn more to show"
      );

      info("Clicking on importable learn more");
      const supportTabPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        Services.urlFormatter.formatURLPref("app.support.baseURL") +
          "password-import"
      );
      EventUtils.synthesizeMouseAtCenter(learnMoreItem, {});
      const supportTab = await supportTabPromise;
      ok(supportTab, "Support tab opened");

      await closePopup(popup);
      BrowserTestUtils.removeTab(supportTab);
    }
  );
});

add_task(async function import_suggestion_not_shown() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function(browser) {
      const popup = document.getElementById("PopupAutoComplete");
      ok(popup, "Got popup");
      let opened = false;
      openACPopup(popup, browser, "#form-basic-password").then(
        () => (opened = true)
      );

      await TestUtils.waitForCondition(() => {
        EventUtils.synthesizeKey("KEY_ArrowDown");
        return opened;
      });

      const footer = popup.querySelector(`[originaltype="loginsFooter"]`);
      ok(footer, "Got footer richlistitem");

      await TestUtils.waitForCondition(() => {
        return !EventUtils.isHidden(footer);
      }, "Waiting for footer to become visible");

      ok(
        !popup.querySelector(`[originaltype="importableLogins"]`),
        "No importable suggestion shown"
      );

      await closePopup(popup);
    }
  );
});
