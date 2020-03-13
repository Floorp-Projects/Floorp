/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint max-len: ["error", 80] */

loadTestSubscript("head_abuse_report.js");

const TESTPAGE = `${SECURE_TESTROOT}webapi_checkavailable.html`;
const TELEMETRY_EVENTS_FILTERS = {
  category: "addonsManager",
  method: "report",
};
const REPORT_PROP_NAMES = [
  "addon",
  "addon_signature",
  "reason",
  "message",
  "report_entry_point",
];

function getObjectProps(obj, propNames) {
  const res = {};
  for (const k of propNames) {
    res[k] = obj[k];
  }
  return res;
}

async function assertSubmittedReport(expectedReportProps) {
  let reportSubmitted;
  const onReportSubmitted = AbuseReportTestUtils.promiseReportSubmitHandled(
    ({ data, request, response }) => {
      reportSubmitted = JSON.parse(data);
      handleSubmitRequest({ request, response });
    }
  );

  let panelEl = await AbuseReportTestUtils.promiseReportDialogRendered();

  let promiseWinClosed = waitClosedWindow();
  let promisePanelUpdated = AbuseReportTestUtils.promiseReportUpdated(
    panelEl,
    "submit"
  );
  panelEl._form.elements.reason.value = expectedReportProps.reason;
  AbuseReportTestUtils.clickPanelButton(panelEl._btnNext);
  await promisePanelUpdated;

  panelEl._form.elements.message.value = expectedReportProps.message;
  // Reset the timestamp of the last report between tests.
  AbuseReporter._lastReportTimestamp = null;
  AbuseReportTestUtils.clickPanelButton(panelEl._btnSubmit);
  await Promise.all([onReportSubmitted, promiseWinClosed]);

  ok(!panelEl.ownerGlobal, "Report dialog window is closed");
  Assert.deepEqual(
    getObjectProps(reportSubmitted, REPORT_PROP_NAMES),
    expectedReportProps,
    "Got the expected report data submitted"
  );
}

add_task(async function setup() {
  await AbuseReportTestUtils.setup();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.webapi.testing", true],
      ["extensions.abuseReport.amWebAPI.enabled", true],
    ],
  });
});

add_task(async function test_report_installed_addon_cancelled() {
  Services.telemetry.clearEvents();

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const extension = await installTestExtension(ADDON_ID);

    let reportEnabled = await SpecialPowers.spawn(browser, [], () => {
      return content.navigator.mozAddonManager.abuseReportPanelEnabled;
    });

    is(reportEnabled, true, "Expect abuseReportPanelEnabled to be true");

    info("Test reportAbuse result on user cancelled report");

    let promiseNewWindow = waitForNewWindow();
    let promiseWebAPIResult = SpecialPowers.spawn(
      browser,
      [ADDON_ID],
      addonId => content.navigator.mozAddonManager.reportAbuse(addonId)
    );

    let win = await promiseNewWindow;
    is(win, AbuseReportTestUtils.getReportDialog(), "Got the report dialog");

    let panelEl = await AbuseReportTestUtils.promiseReportDialogRendered();

    let promiseWinClosed = waitClosedWindow();
    AbuseReportTestUtils.clickPanelButton(panelEl._btnCancel);
    let reportResult = await promiseWebAPIResult;
    is(
      reportResult,
      false,
      "Expect reportAbuse to resolve to false on user cancelled report"
    );
    await promiseWinClosed;
    ok(!panelEl.ownerGlobal, "Report dialog window is closed");

    await extension.unload();
  });

  // Expect no telemetry events collected for user cancelled reports.
  TelemetryTestUtils.assertEvents([], TELEMETRY_EVENTS_FILTERS);
});

add_task(async function test_report_installed_addon_submitted() {
  Services.telemetry.clearEvents();

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const extension = await installTestExtension(ADDON_ID);

    let promiseNewWindow = waitForNewWindow();
    let promiseWebAPIResult = SpecialPowers.spawn(browser, [ADDON_ID], id =>
      content.navigator.mozAddonManager.reportAbuse(id)
    );
    let win = await promiseNewWindow;
    is(win, AbuseReportTestUtils.getReportDialog(), "Got the report dialog");

    await assertSubmittedReport({
      addon: ADDON_ID,
      addon_signature: "missing",
      message: "fake report message",
      reason: "unwanted",
      report_entry_point: "amo",
    });

    let reportResult = await promiseWebAPIResult;
    is(
      reportResult,
      true,
      "Expect reportAbuse to resolve to false on user cancelled report"
    );

    await extension.unload();
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "amo",
        value: ADDON_ID,
        extra: { addon_type: "extension" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_report_unknown_not_installed_addon() {
  const addonId = "unknown-addon@mochi.test";
  Services.telemetry.clearEvents();

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    let promiseWebAPIResult = SpecialPowers.spawn(browser, [addonId], id =>
      content.navigator.mozAddonManager.reportAbuse(id).catch(err => {
        return { name: err.name, message: err.message };
      })
    );

    await Assert.deepEqual(
      await promiseWebAPIResult,
      { name: "Error", message: "Error creating abuse report" },
      "Got the expected rejected error on reporting unknown addon"
    );

    ok(!AbuseReportTestUtils.getReportDialog(), "No report dialog is open");
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "amo",
        value: addonId,
        extra: { error_type: "ERROR_AMODETAILS_NOTFOUND" },
      },
      {
        object: "amo",
        value: addonId,
        extra: { error_type: "ERROR_ADDON_NOTFOUND" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_report_not_installed_addon() {
  const addonId = "not-installed-addon@mochi.test";
  Services.telemetry.clearEvents();

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const fakeAMODetails = {
      name: "fake name",
      current_version: { version: "1.0" },
      type: "extension",
      icon_url: "http://test.addons.org/asserts/fake-icon-url.png",
      homepage: "http://fake.url/homepage",
      authors: [{ name: "author1", url: "http://fake.url/author1" }],
      is_recommended: false,
    };

    AbuseReportTestUtils.amoAddonDetailsMap.set(addonId, fakeAMODetails);
    registerCleanupFunction(() =>
      AbuseReportTestUtils.amoAddonDetailsMap.clear()
    );

    let promiseNewWindow = waitForNewWindow();

    let promiseWebAPIResult = SpecialPowers.spawn(browser, [addonId], id =>
      content.navigator.mozAddonManager.reportAbuse(id)
    );
    let win = await promiseNewWindow;
    is(win, AbuseReportTestUtils.getReportDialog(), "Got the report dialog");

    await assertSubmittedReport({
      addon: addonId,
      addon_signature: "unknown",
      message: "fake report message",
      reason: "other",
      report_entry_point: "amo",
    });

    let reportResult = await promiseWebAPIResult;
    is(
      reportResult,
      true,
      "Expect reportAbuse to resolve to true on submitted report"
    );
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "amo",
        value: addonId,
        extra: { addon_type: "extension" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_amo_report_on_report_already_inprogress() {
  const extension = await installTestExtension(ADDON_ID);
  const reportDialog = await AbuseReporter.openDialog(
    ADDON_ID,
    "menu",
    gBrowser.selectedBrowser
  );
  await AbuseReportTestUtils.promiseReportDialogRendered();
  ok(reportDialog.window, "Got an open report dialog");

  let promiseWinClosed = waitClosedWindow();

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const promiseAMOResult = SpecialPowers.spawn(browser, [ADDON_ID], id =>
      content.navigator.mozAddonManager.reportAbuse(id)
    );

    await promiseWinClosed;
    ok(reportDialog.window.closed, "previous report dialog should be closed");

    is(
      await reportDialog.promiseAMOResult,
      undefined,
      "old report cancelled after AMO called mozAddonManager.reportAbuse"
    );

    const panelEl = await AbuseReportTestUtils.promiseReportDialogRendered();

    const { report } = AbuseReportTestUtils.getReportDialogParams();
    Assert.deepEqual(
      {
        reportEntryPoint: report.reportEntryPoint,
        addonId: report.addon.id,
      },
      {
        reportEntryPoint: "amo",
        addonId: ADDON_ID,
      },
      "Got the expected report from the opened report dialog"
    );

    promiseWinClosed = waitClosedWindow();
    AbuseReportTestUtils.clickPanelButton(panelEl._btnCancel);
    await promiseWinClosed;

    is(
      await promiseAMOResult,
      false,
      "AMO report request resolved to false on cancel button clicked"
    );
  });

  await extension.unload();
});

add_task(async function test_reject_on_unsupported_addon_types() {
  const addonId = "not-supported-addon-type@mochi.test";

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    const fakeAMODetails = {
      name: "fake name",
      current_version: { version: "1.0" },
      type: "fake-unsupported-addon-type",
    };

    AbuseReportTestUtils.amoAddonDetailsMap.set(addonId, fakeAMODetails);
    registerCleanupFunction(() =>
      AbuseReportTestUtils.amoAddonDetailsMap.clear()
    );

    let webAPIResult = await SpecialPowers.spawn(browser, [addonId], id =>
      content.navigator.mozAddonManager.reportAbuse(id).then(
        res => ({ gotRejection: false, result: res }),
        err => ({ gotRejection: true, message: err.message })
      )
    );

    Assert.deepEqual(
      webAPIResult,
      { gotRejection: true, message: "Error creating abuse report" },
      "Got the expected rejection from mozAddonManager.reportAbuse"
    );
  });
});

add_task(async function test_report_on_disabled_webapi() {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.abuseReport.amWebAPI.enabled", false]],
  });

  await BrowserTestUtils.withNewTab(TESTPAGE, async browser => {
    let reportEnabled = await SpecialPowers.spawn(browser, [], () => {
      return content.navigator.mozAddonManager.abuseReportPanelEnabled;
    });

    is(reportEnabled, false, "Expect abuseReportPanelEnabled to be false");

    info("Test reportAbuse result on report webAPI disabled");

    let promiseWebAPIResult = SpecialPowers.spawn(
      browser,
      ["an-addon@mochi.test"],
      addonId =>
        content.navigator.mozAddonManager.reportAbuse(addonId).catch(err => {
          return { name: err.name, message: err.message };
        })
    );

    Assert.deepEqual(
      await promiseWebAPIResult,
      { name: "Error", message: "amWebAPI reportAbuse not supported" },
      "Got the expected rejected error"
    );
  });

  await SpecialPowers.popPrefEnv();
});
