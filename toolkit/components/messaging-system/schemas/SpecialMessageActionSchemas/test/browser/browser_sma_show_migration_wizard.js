/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async () => {
  // Load the initial tab at example.com. This makes it so that if
  // when loading the migration wizard in about:preferences, we'll
  // load the about:preferences page in a new tab rather than overtaking
  // the initial one. This makes cleanup of that opened tab more explicit.
  let browser = gBrowser.selectedBrowser;
  BrowserTestUtils.startLoadingURIString(browser, "https://example.com");
  await BrowserTestUtils.browserLoaded(browser);
});

add_task(async function test_SHOW_MIGRATION_WIZARD() {
  let wizardOpened = BrowserTestUtils.waitForMigrationWizard(window);

  await SMATestUtils.executeAndValidateAction({
    type: "SHOW_MIGRATION_WIZARD",
  });

  let wizard = await wizardOpened;
  ok(wizard, "Migration wizard opened");
  await BrowserTestUtils.removeTab(wizard);
});

add_task(async function test_SHOW_MIGRATION_WIZARD_WITH_SOURCE() {
  let wizardOpened = BrowserTestUtils.waitForMigrationWizard(window);

  await SMATestUtils.executeAndValidateAction({
    type: "SHOW_MIGRATION_WIZARD",
    data: { source: "chrome" },
  });

  let wizard = await wizardOpened;
  ok(wizard, "Migrator window opened when source param specified");
  await BrowserTestUtils.removeTab(wizard);
});
