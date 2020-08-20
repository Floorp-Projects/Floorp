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

add_task(async function import_suggestion_wizard() {
  sinon.stub(ChromeMigrationUtils, "getImportableLogins").resolves(["chrome"]);

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

  ChromeMigrationUtils.getImportableLogins.restore();
});

add_task(async function import_suggestion_learn_more() {
  sinon.stub(ChromeMigrationUtils, "getImportableLogins").resolves(["chrome"]);

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

  ChromeMigrationUtils.getImportableLogins.restore();
});
