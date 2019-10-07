/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_task(async function setup() {
  await AbuseReportTestUtils.setup();

  // Run abuse reports tests in the "dialog window" mode.
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.openDialog", true]],
  });
});

// Add all the test tasks shared with browser_html_abuse_report.js.
addCommonAbuseReportTestTasks();

/**
 * Test tasks specific to the abuse report opened in its own dialog window.
 */

add_task(async function test_report_triggered_when_report_dialog_is_open() {
  const addonId = "addon-to-report@mochi.test";
  const extension = await installTestExtension(addonId);

  const reportDialog = await AbuseReporter.openDialog(
    addonId,
    "menu",
    gBrowser.selectedBrowser
  );
  await AbuseReportTestUtils.promiseReportDialogRendered();

  let promiseClosedWindow = waitClosedWindow();

  const reportDialog2 = await AbuseReporter.openDialog(
    addonId,
    "menu",
    gBrowser.selectedBrowser
  );

  await promiseClosedWindow;

  // Trigger the report submit and check that the second report is
  // resolved as expected.
  await AbuseReportTestUtils.promiseReportDialogRendered();

  ok(
    !reportDialog.window || reportDialog.window.closed,
    "expect the first dialog to be closed"
  );
  ok(!!reportDialog2.window, "expect the second dialog to be open");

  is(
    reportDialog2.window,
    AbuseReportTestUtils.getReportDialog(),
    "Got a report dialog as expected"
  );

  AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");

  // promiseReport is resolved to undefined if the report has been
  // cancelled, otherwise it is resolved to a report object.
  ok(
    !(await reportDialog.promiseReport),
    "expect the first report to be cancelled"
  );
  ok(
    !!(await reportDialog2.promiseReport),
    "expect the second report to be resolved"
  );

  await extension.unload();
});

add_task(async function test_report_dialog_window_closed_by_user() {
  const addonId = "addon-to-report@mochi.test";
  const extension = await installTestExtension(addonId);

  const reportDialog = await AbuseReporter.openDialog(
    addonId,
    "menu",
    gBrowser.selectedBrowser
  );
  await AbuseReportTestUtils.promiseReportDialogRendered();

  let promiseClosedWindow = waitClosedWindow();

  reportDialog.close();

  await promiseClosedWindow;

  ok(
    !(await reportDialog.promiseReport),
    "expect promiseReport to be resolved as user cancelled"
  );

  await extension.unload();
});
