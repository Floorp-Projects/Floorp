/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

add_task(async function setup() {
  await AbuseReportTestUtils.setup();
});

/**
 * Test tasks specific to the abuse report opened in its own dialog window.
 */

add_task(async function test_close_icon_button_hidden_when_dialog() {
  const addonId = "addon-to-report@mochi.test";
  const extension = await installTestExtension(addonId);

  const reportDialog = await AbuseReporter.openDialog(
    addonId,
    "menu",
    gBrowser.selectedBrowser
  );
  await AbuseReportTestUtils.promiseReportDialogRendered();

  const panelEl = await reportDialog.promiseReportPanel;

  let promiseClosedWindow = waitClosedWindow();

  EventUtils.synthesizeKey("VK_RETURN", {}, panelEl.ownerGlobal);
  AbuseReportTestUtils.triggerSubmit("fake-reason", "fake-message");

  await promiseClosedWindow;
  ok(
    await reportDialog.promiseReport,
    "expect the report to not be cancelled by pressing enter"
  );

  await extension.unload();
});

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

add_task(async function test_amo_details_for_not_installed_addon() {
  const addonId = "not-installed-addon@mochi.test";
  const fakeAMODetails = {
    name: "fake name",
    current_version: { version: "1.0" },
    type: "extension",
    icon_url: "http://test.addons.org/asserts/fake-icon-url.png",
    homepage: "http://fake.url/homepage",
    support_url: "http://fake.url/support",
    authors: [
      { name: "author1", url: "http://fake.url/author1" },
      { name: "author2", url: "http://fake.url/author2" },
    ],
    is_recommended: true,
  };

  AbuseReportTestUtils.amoAddonDetailsMap.set(addonId, fakeAMODetails);
  registerCleanupFunction(() =>
    AbuseReportTestUtils.amoAddonDetailsMap.clear()
  );

  const reportDialog = await AbuseReporter.openDialog(
    addonId,
    "menu",
    gBrowser.selectedBrowser
  );

  const reportEl = await reportDialog.promiseReportPanel;

  // Assert that the panel has been able to retrieve from AMO
  // all the addon details needed to render the panel correctly.
  is(reportEl.addonId, addonId, "Got the expected addonId");
  is(reportEl.addonName, fakeAMODetails.name, "Got the expected addon name");
  is(reportEl.addonType, fakeAMODetails.type, "Got the expected addon type");
  is(
    reportEl.authorName,
    fakeAMODetails.authors[0].name,
    "Got the first author name as expected"
  );
  is(
    reportEl.authorURL,
    fakeAMODetails.authors[0].url,
    "Got the first author url as expected"
  );
  is(reportEl.iconURL, fakeAMODetails.icon_url, "Got the expected icon url");
  is(
    reportEl.supportURL,
    fakeAMODetails.support_url,
    "Got the expected support url"
  );
  is(
    reportEl.homepageURL,
    fakeAMODetails.homepage,
    "Got the expected homepage url"
  );

  reportDialog.close();
});
