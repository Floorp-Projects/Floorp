/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  // Load the initial tab at example.com. This makes it so that if
  // we're using the new migration wizard, we'll load the about:preferences
  // page in a new tab rather than overtaking the initial one. This
  // makes it easier to be consistent with closing and opening
  // behaviours between the two kinds of migration wizards.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.loadURIString(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);
});

add_task(async function test_SHOW_MIGRATION_WIZARD() {
  let wizardOpened = BrowserTestUtils.waitForMigrationWizard(window);

  SMATestUtils.executeAndValidateAction({ type: "SHOW_MIGRATION_WIZARD" });

  let wizard = await wizardOpened;
  ok(wizard, "Migration wizard opened");
  await BrowserTestUtils.closeMigrationWizard(wizard);
});

add_task(async function test_SHOW_MIGRATION_WIZARD_WITH_SOURCE() {
  let wizardOpened = BrowserTestUtils.waitForMigrationWizard(window);

  SMATestUtils.executeAndValidateAction({
    type: "SHOW_MIGRATION_WIZARD",
    data: { source: "chrome" },
  });

  let wizard = await wizardOpened;
  ok(wizard, "Migrator window opened when source param specified");
  await BrowserTestUtils.closeMigrationWizard(wizard);
});
