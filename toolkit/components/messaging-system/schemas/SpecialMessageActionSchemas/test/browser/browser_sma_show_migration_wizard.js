/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MigrationUtils } = ChromeUtils.import(
  "resource:///modules/MigrationUtils.jsm"
);

add_task(async function test_SHOW_MIGRATION_WIZARD() {
  let migratorOpen = TestUtils.waitForCondition(() => {
    let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
    return win && win.document && win.document.readyState == "complete";
  }, "Migrator window loaded");

  SMATestUtils.executeAndValidateAction({ type: "SHOW_MIGRATION_WIZARD" });

  await migratorOpen;
  let migratorWindow = Services.wm.getMostRecentWindow(
    "Browser:MigrationWizard"
  );
  ok(migratorWindow, "Migrator window opened");
  await BrowserTestUtils.closeWindow(migratorWindow);
});

add_task(async function test_SHOW_MIGRATION_WIZARD_WITH_SOURCE() {
  let migratorOpen = TestUtils.waitForCondition(() => {
    let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
    return win && win.document && win.document.readyState == "complete";
  }, "Migrator window loaded");

  SMATestUtils.executeAndValidateAction({
    type: "SHOW_MIGRATION_WIZARD",
    data: { source: "chrome" },
  });

  await migratorOpen;
  let migratorWindow = Services.wm.getMostRecentWindow(
    "Browser:MigrationWizard"
  );
  ok(migratorWindow, "Migrator window opened when source param specified");
  await BrowserTestUtils.closeWindow(migratorWindow);
});
