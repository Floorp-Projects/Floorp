/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var doc, tbody, tabAboutProcesses;

const rowTypes = ["process", "window", "thread-summary", "thread"];

function promiseUpdate() {
  return promiseAboutProcessesUpdated({
    doc,
    tbody,
    force: true,
    tabAboutProcesses,
  });
}

add_setup(async function () {
  Services.prefs.setBoolPref("toolkit.aboutProcesses.showThreads", true);

  info("Setting up about:processes");
  tabAboutProcesses = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: "about:processes",
    waitForLoad: true,
  });

  doc = tabAboutProcesses.linkedBrowser.contentDocument;
  tbody = doc.getElementById("process-tbody");
  await promiseUpdate();

  info("Open a list of threads to have thread rows displayed");
  let twisty = doc.querySelector("tr.thread-summary .twisty");
  twisty.click();
  await promiseUpdate();
});

add_task(async function testSelectionPersistedAfterUpdate() {
  for (let rowType of rowTypes) {
    let row = doc.querySelector(`tr.${rowType}`);
    Assert.ok(!!row, `Found ${rowType} row`);
    Assert.ok(!row.hasAttribute("selected"), "The row should not be selected");

    info("Click in the row to select it.");
    row.click();
    Assert.equal(
      row.getAttribute("selected"),
      "true",
      "The row should be selected"
    );
    Assert.equal(
      doc.querySelectorAll("[selected]").length,
      1,
      "There should be only one selected row"
    );

    info("Wait for an update and ensure the selected row is still the same");
    let rowId = row.rowId;
    let findRowsWithId = rowId =>
      [...doc.querySelectorAll("tr")].filter(r => r.rowId == rowId);
    Assert.equal(
      findRowsWithId(rowId).length,
      1,
      "There should be only one row with id " + rowId
    );
    await promiseUpdate();
    let selectedRow = doc.querySelector("[selected]");
    if (rowType == "thread" && !selectedRow) {
      info("The thread row might have disappeared if the thread has ended");
      Assert.equal(
        findRowsWithId(rowId).length,
        0,
        "There should no longer be a row with id " + rowId
      );
      continue;
    }
    Assert.ok(
      !!selectedRow,
      "There should still be a selected row after an update"
    );
    Assert.equal(
      selectedRow.rowId,
      rowId,
      "The selected row should have the same id as the row we clicked"
    );
  }
});

add_task(function testClickAgainToRemoveSelection() {
  for (let rowType of rowTypes) {
    let row = doc.querySelector(`tr.${rowType}`);
    Assert.ok(!!row, `Found ${rowType} row`);
    Assert.ok(!row.hasAttribute("selected"), "The row should not be selected");
    info("Click in the row to select it.");
    row.click();
    Assert.equal(
      row.getAttribute("selected"),
      "true",
      "The row should now be selected"
    );
    Assert.equal(
      doc.querySelectorAll("[selected]").length,
      1,
      "There should be only one selected row"
    );

    info("Click the row again to remove the selection.");
    row.click();
    Assert.ok(
      !row.hasAttribute("selected"),
      "The row should no longer be selected"
    );
    Assert.ok(
      !doc.querySelector("[selected]"),
      "There should be no selected row"
    );
  }
});

add_task(function cleanup() {
  BrowserTestUtils.removeTab(tabAboutProcesses);
  Services.prefs.clearUserPref("toolkit.aboutProcesses.showThreads");
});
