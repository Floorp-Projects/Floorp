/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * We currently support both the legacy and the new migration wizard. The
 * legacy migration wizard opens in a top-level XUL dialog. The new migration
 * wizard opens as an HTML dialog within about:preferences.
 *
 * This variable is true if the new migration wizard that loads in
 * about:preferences is being used.
 *
 * The legacy wizard codepaths can be removed once bug 1824851 lands.
 */
const USING_LEGACY_WIZARD = !Services.prefs.getBoolPref(
  "browser.migrate.content-modal.enabled",
  false
);

/**
 * A helper function for this test that returns a Promise that resolves
 * once either the legacy or new migration wizard appears.
 *
 * @returns {Promise<Element>}
 *   Resolves to the dialog window in the legacy case, and the
 *   about:preferences tab otherwise.
 */
async function waitForWizard() {
  if (USING_LEGACY_WIZARD) {
    return BrowserTestUtils.waitForCondition(
      () => Services.wm.getMostRecentWindow("Browser:MigrationWizard"),
      "Wait for migration wizard to open"
    );
  }

  let wizardReady = BrowserTestUtils.waitForEvent(
    window,
    "MigrationWizard:Ready"
  );
  let wizardTab = await BrowserTestUtils.waitForNewTab(gBrowser, url => {
    return url.startsWith("about:preferences");
  });
  await wizardReady;

  return wizardTab;
}

/**
 * Closes the migration wizard.
 *
 * @param {Element} wizardWindowOrTab
 *   The XUL dialog window for the migration wizard in the legacy case, and
 *   the about:preferences tab otherwise.
 * @returns {Promise<undefined>}
 */
function closeWizard(wizardWindowOrTab) {
  if (USING_LEGACY_WIZARD) {
    return BrowserTestUtils.closeWindow(wizardWindowOrTab);
  }

  return BrowserTestUtils.removeTab(wizardWindowOrTab);
}

add_setup(async () => {
  // Load the initial tab at example.com. This makes it so that if
  // USING_LEGACY_WIZARD is false, we'll load the about:preferences
  // page in a new tab rather than overtaking the initial one. This
  // makes it easier to be consistent with closing and opening
  // behaviours between the two kinds of migration wizards.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.loadURIString(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);
});

add_task(async function test_SHOW_MIGRATION_WIZARD() {
  let wizardOpened = waitForWizard();

  SMATestUtils.executeAndValidateAction({ type: "SHOW_MIGRATION_WIZARD" });

  let wizard = await wizardOpened;
  ok(wizard, "Migration wizard opened");
  closeWizard(wizard);
});

add_task(async function test_SHOW_MIGRATION_WIZARD_WITH_SOURCE() {
  let wizardOpened = waitForWizard();

  SMATestUtils.executeAndValidateAction({
    type: "SHOW_MIGRATION_WIZARD",
    data: { source: "chrome" },
  });

  let wizard = await wizardOpened;
  ok(wizard, "Migrator window opened when source param specified");
  closeWizard(wizard);
});
