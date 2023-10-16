const { ChromeMigrationUtils } = ChromeUtils.importESModule(
  "resource:///modules/ChromeMigrationUtils.sys.mjs"
);
const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

// Dummy migrator to change and detect importable behavior.
const gTestMigrator = {
  profiles: [],

  getSourceProfiles() {
    return this.profiles;
  },

  migrate: sinon
    .stub()
    .callsFake(() =>
      LoginTestUtils.addLogin({ username: "import", password: "pass" })
    ),
};

// Showing importables updates counts delayed, so adjust and cleanup.
add_setup(async function setup() {
  const debounce = sinon
    .stub(LoginManagerParent, "SUGGEST_IMPORT_DEBOUNCE_MS")
    .value(0);
  const importable = sinon
    .stub(ChromeMigrationUtils, "getImportableLogins")
    .resolves(["chrome"]);
  const migrator = sinon
    .stub(MigrationUtils, "getMigrator")
    .resolves(gTestMigrator);

  const doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
    featureId: "password-autocomplete",
    value: { directMigrateSingleProfile: true },
  });

  // This makes the last autocomplete test *not* show import suggestions.
  Services.prefs.setIntPref("signon.suggestImportCount", 3);

  registerCleanupFunction(async () => {
    await doExperimentCleanup();
    debounce.restore();
    importable.restore();
    migrator.restore();
    Services.prefs.clearUserPref("signon.suggestImportCount");
  });
});

add_task(async function check_fluent_ids() {
  await document.l10n.ready;
  MozXULElement.insertFTLIfNeeded("toolkit/main-window/autocomplete.ftl");

  const host = "testhost.com";
  for (const browser of ChromeMigrationUtils.CONTEXTUAL_LOGIN_IMPORT_BROWSERS) {
    const id = `autocomplete-import-logins-${browser}`;
    const message = await document.l10n.formatValue(id, { host });
    Assert.ok(
      message.includes(`data-l10n-name="line1"`),
      `${id} included line1`
    );
    Assert.ok(
      message.includes(`data-l10n-name="line2"`),
      `${id} included line2`
    );
    Assert.ok(message.includes(host), `${id} replaced host`);
  }
});

/**
 * Tests that if the user selects the password import suggestion from
 * the autocomplete popup, and there is more than one profile available
 * to import from, that the migration wizard opens to guide the user
 * through importing those logins.
 */
add_task(async function import_suggestion_wizard() {
  let wizardTab;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function (browser) {
      const popup = document.getElementById("PopupAutoComplete");
      Assert.ok(popup, "Got popup");
      await openACPopup(popup, browser, "#form-basic-username");

      const importableItem = popup.querySelector(
        `[originaltype="importableLogins"]`
      );
      Assert.ok(importableItem, "Got importable suggestion richlistitem");

      await BrowserTestUtils.waitForCondition(
        () => !importableItem.collapsed,
        "Wait for importable suggestion to show"
      );

      // Pretend there's 2+ profiles to trigger the wizard.
      gTestMigrator.profiles.length = 2;

      info("Clicking on importable suggestion");
      const wizardPromise = BrowserTestUtils.waitForMigrationWizard(window);

      EventUtils.synthesizeMouseAtCenter(importableItem, {});

      wizardTab = await wizardPromise;
      Assert.ok(wizardTab, "Wizard opened");
      Assert.equal(
        gTestMigrator.migrate.callCount,
        0,
        "Direct migrate not used"
      );

      await closePopup(popup);
    }
  );

  // Close the wizard tab in the end of the test. If we close the wizard when the tab
  // is still opened, the username field will be focused again, which triggers another
  // importable suggestion.
  await BrowserTestUtils.removeTab(wizardTab);
});

add_task(async function import_suggestion_learn_more() {
  let supportTab;
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function (browser) {
      const popup = document.getElementById("PopupAutoComplete");
      Assert.ok(popup, "Got popup");
      await openACPopup(popup, browser, "#form-basic-username");

      const learnMoreItem = popup.querySelector(`[type="importableLearnMore"]`);
      Assert.ok(learnMoreItem, "Got importable learn more richlistitem");

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
      supportTab = await supportTabPromise;
      Assert.ok(supportTab, "Support tab opened");

      await closePopup(popup);
    }
  );

  // Close the tab in the end of the test to avoid the username field being
  // focused again.
  await BrowserTestUtils.removeTab(supportTab);
});

add_task(async function import_suggestion_migrate() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function (browser) {
      const popup = document.getElementById("PopupAutoComplete");
      Assert.ok(popup, "Got popup");
      await openACPopup(popup, browser, "#form-basic-username");

      const importableItem = popup.querySelector(
        `[originaltype="importableLogins"]`
      );
      Assert.ok(importableItem, "Got importable suggestion richlistitem");

      await BrowserTestUtils.waitForCondition(
        () => !importableItem.collapsed,
        "Wait for importable suggestion to show"
      );

      // Pretend there's 1 profile to trigger migrate.
      gTestMigrator.profiles.length = 1;

      info("Clicking on importable suggestion");
      const migratePromise = BrowserTestUtils.waitForCondition(
        () => gTestMigrator.migrate.callCount,
        "Wait for direct migration attempt"
      );
      EventUtils.synthesizeMouseAtCenter(importableItem, {});

      const callCount = await migratePromise;
      Assert.equal(callCount, 1, "Direct migrate used once");

      const importedItem = await BrowserTestUtils.waitForCondition(
        () => popup.querySelector(`[originaltype="loginWithOrigin"]`),
        "Wait for imported login to show"
      );
      EventUtils.synthesizeMouseAtCenter(importedItem, {});

      const username = await SpecialPowers.spawn(
        browser,
        [],
        () => content.document.getElementById("form-basic-username").value
      );
      Assert.equal(username, "import", "username from import filled in");

      LoginTestUtils.clearData();
    }
  );
});

add_task(async function import_suggestion_not_shown() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "https://example.com" + DIRECTORY_PATH + "form_basic.html",
    },
    async function (browser) {
      const popup = document.getElementById("PopupAutoComplete");
      Assert.ok(popup, "Got popup");
      let opened = false;
      openACPopup(popup, browser, "#form-basic-password").then(
        () => (opened = true)
      );

      await TestUtils.waitForCondition(() => {
        EventUtils.synthesizeKey("KEY_ArrowDown");
        return opened;
      });

      const footer = popup.querySelector(`[originaltype="loginsFooter"]`);
      Assert.ok(footer, "Got footer richlistitem");

      await TestUtils.waitForCondition(() => {
        return !EventUtils.isHidden(footer);
      }, "Waiting for footer to become visible");

      Assert.ok(
        !popup.querySelector(`[originaltype="importableLogins"]`),
        "No importable suggestion shown"
      );

      await closePopup(popup);
    }
  );
});
