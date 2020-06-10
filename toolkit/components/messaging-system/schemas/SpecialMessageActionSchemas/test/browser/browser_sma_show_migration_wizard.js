/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { MigrationUtils } = ChromeUtils.import(
  "resource:///modules/MigrationUtils.jsm"
);

add_task(async function test_OPEN_ABOUT_PAGE() {
  let migratorOpen = TestUtils.waitForCondition(() => {
    let win = Services.wm.getMostRecentWindow("Browser:MigrationWizard");
    return win && win.document && win.document.readyState == "complete";
  }, "Migrator window loaded");

  // We can't call this code directly or our JS execution will get blocked on Windows/Linux where
  // the dialog is modal.
  executeSoon(() =>
    SMATestUtils.executeAndValidateAction({ type: "SHOW_MIGRATION_WIZARD" })
  );

  await migratorOpen;
  let migratorWindow = Services.wm.getMostRecentWindow(
    "Browser:MigrationWizard"
  );
  ok(migratorWindow, "Migrator window opened");
  await BrowserTestUtils.closeWindow(migratorWindow);
});
