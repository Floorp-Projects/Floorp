/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AbuseReporter, AbuseReportError } = ChromeUtils.import(
  "resource://gre/modules/AbuseReporter.jsm"
);

const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { L10nRegistry, FileSource } = ChromeUtils.import(
  "resource://gre/modules/L10nRegistry.jsm"
);

const APPNAME = "XPCShell";
const APPVERSION = "1";
const ADDON_ID = "test-addon@tests.mozilla.org";
const ADDON_ID2 = "test-addon2@tests.mozilla.org";
const FAKE_INSTALL_INFO = {
  source: "fake-Install:Source",
  method: "fake:install method",
};
const PREF_REQUIRED_LOCALE = "intl.locale.requested";
const REPORT_OPTIONS = { reportEntryPoint: "menu" };
const TELEMETRY_EVENTS_FILTERS = {
  category: "addonsManager",
  method: "report",
};

const FAKE_AMO_DETAILS = {
  name: {
    "en-US": "fake name",
    "it-IT": "fake it-IT name",
  },
  current_version: { version: "1.0" },
  type: "extension",
  is_recommended: true,
};

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer({ hosts: ["test.addons.org"] });

// Mock abuse report API endpoint.
let apiRequestHandler;
server.registerPathHandler("/api/report/", (request, response) => {
  const stream = request.bodyInputStream;
  const buffer = NetUtil.readInputStream(stream, stream.available());
  const data = new TextDecoder().decode(buffer);
  apiRequestHandler({ data, request, response });
});

// Mock addon details API endpoint.
const amoAddonDetailsMap = new Map();
server.registerPrefixHandler("/api/addons/addon/", (request, response) => {
  const addonId = request.path.split("/").pop();
  if (!amoAddonDetailsMap.has(addonId)) {
    response.setStatusLine(request.httpVersion, 404, "Not Found");
    response.write(JSON.stringify({ detail: "Not found." }));
  } else {
    response.setStatusLine(request.httpVersion, 200, "Success");
    response.write(JSON.stringify(amoAddonDetailsMap.get(addonId)));
  }
});

function getProperties(obj, propNames) {
  return propNames.reduce((acc, el) => {
    acc[el] = obj[el];
    return acc;
  }, {});
}

function handleSubmitRequest({ request, response }) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.write("{}");
}

function clearAbuseReportState() {
  // Clear the timestamp of the last submission.
  AbuseReporter._lastReportTimestamp = null;
}

async function installTestExtension(overrideOptions = {}) {
  const extOptions = {
    manifest: {
      applications: { gecko: { id: ADDON_ID } },
      name: "Test Extension",
    },
    useAddonManager: "permanent",
    amInstallTelemetryInfo: FAKE_INSTALL_INFO,
    ...overrideOptions,
  };

  const extension = ExtensionTestUtils.loadExtension(extOptions);
  await extension.startup();

  const addon = await AddonManager.getAddonByID(ADDON_ID);

  return { extension, addon };
}

async function assertRejectsAbuseReportError(promise, errorType, errorInfo) {
  let error;

  await Assert.rejects(
    promise,
    err => {
      // Log the actual error to make investigating test failures easier.
      Cu.reportError(err);
      error = err;
      return err instanceof AbuseReportError;
    },
    `Got an AbuseReportError`
  );

  equal(error.errorType, errorType, "Got the expected errorType");
  equal(error.errorInfo, errorInfo, "Got the expected errorInfo");
  ok(
    error.message.includes(errorType),
    "errorType should be included in the error message"
  );
  if (errorInfo) {
    ok(
      error.message.includes(errorInfo),
      "errorInfo should be included in the error message"
    );
  }
}

async function assertBaseReportData({ reportData, addon }) {
  // Report properties related to addon metadata.
  equal(reportData.addon, ADDON_ID, "Got expected 'addon'");
  equal(
    reportData.addon_version,
    addon.version,
    "Got expected 'addon_version'"
  );
  equal(
    reportData.install_date,
    addon.installDate.toISOString(),
    "Got expected 'install_date' in ISO format"
  );
  equal(
    reportData.addon_install_origin,
    addon.sourceURI.spec,
    "Got expected 'addon_install_origin'"
  );
  equal(
    reportData.addon_install_source,
    "fake_install_source",
    "Got expected 'addon_install_source'"
  );
  equal(
    reportData.addon_install_method,
    "fake_install_method",
    "Got expected 'addon_install_method'"
  );
  equal(
    reportData.addon_signature,
    "privileged",
    "Got expected 'addon_signature'"
  );

  // Report properties related to the environment.
  equal(
    reportData.client_id,
    await ClientID.getClientIdHash(),
    "Got the expected 'client_id'"
  );
  equal(reportData.app, APPNAME.toLowerCase(), "Got expected 'app'");
  equal(reportData.appversion, APPVERSION, "Got expected 'appversion'");
  equal(
    reportData.lang,
    Services.locale.appLocaleAsBCP47,
    "Got expected 'lang'"
  );
  equal(
    reportData.operating_system,
    AppConstants.platform,
    "Got expected 'operating_system'"
  );
  equal(
    reportData.operating_system_version,
    Services.sysinfo.getProperty("version"),
    "Got expected 'operating_system_version'"
  );
}

add_task(async function test_setup() {
  Services.prefs.setCharPref(
    "extensions.abuseReport.url",
    "http://test.addons.org/api/report/"
  );

  Services.prefs.setCharPref(
    "extensions.abuseReport.amoDetailsURL",
    "http://test.addons.org/api/addons/addon"
  );

  await promiseStartupManager();
  // Telemetry test setup needed to ensure that the builtin events are defined
  // and they can be collected and verified.
  await TelemetryController.testSetup();

  // This is actually only needed on Android, because it does not properly support unified telemetry
  // and so, if not enabled explicitly here, it would make these tests to fail when running on a
  // non-Nightly build.
  const oldCanRecordBase = Services.telemetry.canRecordBase;
  Services.telemetry.canRecordBase = true;
  registerCleanupFunction(() => {
    Services.telemetry.canRecordBase = oldCanRecordBase;
  });

  // Register a fake it-IT locale (used to test localized AMO details in some
  // of the test case defined in this test file).
  L10nRegistry.registerSources([
    new FileSource(
      "mock",
      ["it-IT", "fr-FR"],
      "resource://fake/locales/{locale}"
    ),
  ]);
});

add_task(async function test_addon_report_data() {
  info("Verify report property for a privileged extension");
  const { addon, extension } = await installTestExtension();
  const data = await AbuseReporter.getReportData(addon);
  await assertBaseReportData({ reportData: data, addon });
  await extension.unload();

  info("Verify 'addon_signature' report property for non privileged extension");
  AddonTestUtils.usePrivilegedSignatures = false;
  const { addon: addon2, extension: extension2 } = await installTestExtension();
  const data2 = await AbuseReporter.getReportData(addon2);
  equal(
    data2.addon_signature,
    "signed",
    "Got expected 'addon_signature' for non privileged extension"
  );
  await extension2.unload();

  info("Verify 'addon_install_method' report property on temporary install");
  const { addon: addon3, extension: extension3 } = await installTestExtension({
    useAddonManager: "temporary",
  });
  const data3 = await AbuseReporter.getReportData(addon3);
  equal(
    data3.addon_install_source,
    "temporary_addon",
    "Got expected 'addon_install_method' on temporary install"
  );
  await extension3.unload();
});

add_task(async function test_report_on_not_installed_addon() {
  Services.telemetry.clearEvents();

  // Make sure that the AMO addons details API endpoint is going to
  // return a 404 status for the not installed addon.
  amoAddonDetailsMap.delete(ADDON_ID);

  await assertRejectsAbuseReportError(
    AbuseReporter.createAbuseReport(ADDON_ID, REPORT_OPTIONS),
    "ERROR_ADDON_NOTFOUND"
  );

  TelemetryTestUtils.assertEvents(
    [
      {
        object: REPORT_OPTIONS.reportEntryPoint,
        value: ADDON_ID,
        extra: { error_type: "ERROR_AMODETAILS_NOTFOUND" },
      },
      {
        object: REPORT_OPTIONS.reportEntryPoint,
        value: ADDON_ID,
        extra: { error_type: "ERROR_ADDON_NOTFOUND" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );

  Services.telemetry.clearEvents();
});

// This tests verifies how the addon installTelemetryInfo values are being
// normalized into the addon_install_source and addon_install_method
// expected by the API endpoint.
add_task(async function test_normalized_addon_install_source_and_method() {
  async function assertAddonInstallMethod(amInstallTelemetryInfo, expected) {
    const { addon, extension } = await installTestExtension({
      amInstallTelemetryInfo,
    });
    const {
      addon_install_method,
      addon_install_source,
      addon_install_source_url,
    } = await AbuseReporter.getReportData(addon);

    Assert.deepEqual(
      {
        addon_install_method,
        addon_install_source,
        addon_install_source_url,
      },
      {
        addon_install_method: expected.method,
        addon_install_source: expected.source,
        addon_install_source_url: expected.sourceURL,
      },
      `Got the expected report data for ${JSON.stringify(
        amInstallTelemetryInfo
      )}`
    );
    await extension.unload();
  }

  // Array of testcases: the `test` property contains the installTelemetryInfo value
  // and the `expect` contains the expected normalized values.
  const TEST_CASES = [
    // Explicitly verify normalized values on missing telemetry info.
    {
      test: null,
      expect: { source: null, method: null },
    },

    // Verify expected normalized values for some common install telemetry info.
    {
      test: { source: "about:addons", method: "drag-and-drop" },
      expect: { source: "about_addons", method: "drag_and_drop" },
    },
    {
      test: { source: "amo", method: "amWebAPI" },
      expect: { source: "amo", method: "amwebapi" },
    },
    {
      test: { source: "app-profile", method: "sideload" },
      expect: { source: "app_profile", method: "sideload" },
    },
    {
      test: { source: "distribution" },
      expect: { source: "distribution", method: null },
    },
    {
      test: {
        method: "installTrigger",
        source: "test-host",
        sourceURL: "http://host.triggered.install/example?test=1",
      },
      expect: {
        method: "installtrigger",
        source: "test_host",
        sourceURL: "http://host.triggered.install/example?test=1",
      },
    },
    {
      test: {
        method: "link",
        source: "unknown",
        sourceURL: "https://another.host/installExtension?name=ext1",
      },
      expect: {
        method: "link",
        source: "unknown",
        sourceURL: "https://another.host/installExtension?name=ext1",
      },
    },
  ];

  for (const { expect, test } of TEST_CASES) {
    await assertAddonInstallMethod(test, expect);
  }
});

add_task(async function test_report_create_and_submit() {
  Services.telemetry.clearEvents();

  // Override the test api server request handler, to be able to
  // intercept the submittions to the test api server.
  let reportSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    reportSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  const { addon, extension } = await installTestExtension();

  const reportEntryPoint = "menu";
  const report = await AbuseReporter.createAbuseReport(ADDON_ID, {
    reportEntryPoint,
  });

  equal(report.addon, addon, "Got the expected addon property");
  equal(
    report.reportEntryPoint,
    reportEntryPoint,
    "Got the expected reportEntryPoint"
  );

  const baseReportData = await AbuseReporter.getReportData(addon);
  const reportProperties = {
    message: "test message",
    reason: "test-reason",
  };

  info("Submitting report");
  report.setMessage(reportProperties.message);
  report.setReason(reportProperties.reason);
  await report.submit();

  const expectedEntries = Object.entries({
    report_entry_point: reportEntryPoint,
    ...baseReportData,
    ...reportProperties,
  });

  for (const [expectedKey, expectedValue] of expectedEntries) {
    equal(
      reportSubmitted[expectedKey],
      expectedValue,
      `Got the expected submitted value for "${expectedKey}"`
    );
  }

  TelemetryTestUtils.assertEvents(
    [
      {
        object: reportEntryPoint,
        value: ADDON_ID,
        extra: { addon_type: "extension" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );

  await extension.unload();
});

add_task(async function test_error_recent_submit() {
  Services.telemetry.clearEvents();
  clearAbuseReportState();

  let reportSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    reportSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  const { extension } = await installTestExtension();
  const report = await AbuseReporter.createAbuseReport(ADDON_ID, {
    reportEntryPoint: "uninstall",
  });

  const { extension: extension2 } = await installTestExtension({
    manifest: {
      applications: { gecko: { id: ADDON_ID2 } },
      name: "Test Extension2",
    },
  });
  const report2 = await AbuseReporter.createAbuseReport(
    ADDON_ID2,
    REPORT_OPTIONS
  );

  // Submit the two reports in fast sequence.
  report.setReason("reason1");
  report2.setReason("reason2");
  await report.submit();
  await assertRejectsAbuseReportError(report2.submit(), "ERROR_RECENT_SUBMIT");
  equal(
    reportSubmitted.reason,
    "reason1",
    "Server only received the data from the first submission"
  );

  TelemetryTestUtils.assertEvents(
    [
      {
        object: "uninstall",
        value: ADDON_ID,
        extra: { addon_type: "extension" },
      },
      {
        object: REPORT_OPTIONS.reportEntryPoint,
        value: ADDON_ID2,
        extra: {
          addon_type: "extension",
          error_type: "ERROR_RECENT_SUBMIT",
        },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );

  await extension.unload();
  await extension2.unload();
});

add_task(async function test_submission_server_error() {
  const { extension } = await installTestExtension();

  async function testErrorCode({
    responseStatus,
    responseText = "",
    expectedErrorType,
    expectedErrorInfo,
    expectRequest = true,
  }) {
    info(
      `Test expected AbuseReportError on response status "${responseStatus}"`
    );
    Services.telemetry.clearEvents();
    clearAbuseReportState();

    let requestReceived = false;
    apiRequestHandler = ({ request, response }) => {
      requestReceived = true;
      response.setStatusLine(request.httpVersion, responseStatus, "Error");
      response.write(responseText);
    };

    const report = await AbuseReporter.createAbuseReport(
      ADDON_ID,
      REPORT_OPTIONS
    );
    report.setReason("a-reason");
    const promiseSubmit = report.submit();
    if (typeof expectedErrorType === "string") {
      // Assert a specific AbuseReportError errorType.
      await assertRejectsAbuseReportError(
        promiseSubmit,
        expectedErrorType,
        expectedErrorInfo
      );
    } else {
      // Assert on a given Error class.
      await Assert.rejects(promiseSubmit, expectedErrorType);
    }
    equal(
      requestReceived,
      expectRequest,
      `${expectRequest ? "" : "Not "}received a request as expected`
    );

    TelemetryTestUtils.assertEvents(
      [
        {
          object: REPORT_OPTIONS.reportEntryPoint,
          value: ADDON_ID,
          extra: {
            addon_type: "extension",
            error_type:
              typeof expectedErrorType === "string"
                ? expectedErrorType
                : "ERROR_UNKNOWN",
          },
        },
      ],
      TELEMETRY_EVENTS_FILTERS
    );
  }

  await testErrorCode({
    responseStatus: 500,
    responseText: "A server error",
    expectedErrorType: "ERROR_SERVER",
    expectedErrorInfo: JSON.stringify({
      status: 500,
      responseText: "A server error",
    }),
  });
  await testErrorCode({
    responseStatus: 404,
    responseText: "Not found error",
    expectedErrorType: "ERROR_CLIENT",
    expectedErrorInfo: JSON.stringify({
      status: 404,
      responseText: "Not found error",
    }),
  });
  // Test response with unexpected status code.
  await testErrorCode({
    responseStatus: 604,
    responseText: "An unexpected status code",
    expectedErrorType: "ERROR_UNKNOWN",
    expectedErrorInfo: JSON.stringify({
      status: 604,
      responseText: "An unexpected status code",
    }),
  });
  // Test response status 200 with invalid json data.
  await testErrorCode({
    responseStatus: 200,
    expectedErrorType: /SyntaxError: JSON.parse/,
  });

  // Test on invalid url.
  Services.prefs.setCharPref(
    "extensions.abuseReport.url",
    "invalid-protocol://abuse-report"
  );
  await testErrorCode({
    expectedErrorType: "ERROR_NETWORK",
    expectRequest: false,
  });

  await extension.unload();
});

add_task(async function set_test_abusereport_url() {
  Services.prefs.setCharPref(
    "extensions.abuseReport.url",
    "http://test.addons.org/api/report/"
  );
});

add_task(async function test_submission_aborting() {
  Services.telemetry.clearEvents();
  clearAbuseReportState();

  const { extension } = await installTestExtension();

  // override the api request handler with one that is never going to reply.
  let receivedRequestsCount = 0;
  let resolvePendingResponses;
  const waitToReply = new Promise(
    resolve => (resolvePendingResponses = resolve)
  );

  const onRequestReceived = new Promise(resolve => {
    apiRequestHandler = ({ request, response }) => {
      response.processAsync();
      response.setStatusLine(request.httpVersion, 200, "OK");
      receivedRequestsCount++;
      resolve();

      // Keep the request pending until resolvePendingResponses have been
      // called.
      waitToReply.then(() => {
        response.finish();
      });
    };
  });

  const report = await AbuseReporter.createAbuseReport(
    ADDON_ID,
    REPORT_OPTIONS
  );
  report.setReason("a-reason");
  const promiseResult = report.submit();

  await onRequestReceived;

  ok(receivedRequestsCount > 0, "Got the expected number of requests");
  ok(
    (await Promise.race([promiseResult, Promise.resolve("pending")])) ===
      "pending",
    "Submission fetch request should still be pending"
  );

  report.abort();

  await assertRejectsAbuseReportError(promiseResult, "ERROR_ABORTED_SUBMIT");

  TelemetryTestUtils.assertEvents(
    [
      {
        object: REPORT_OPTIONS.reportEntryPoint,
        value: ADDON_ID,
        extra: { addon_type: "extension", error_type: "ERROR_ABORTED_SUBMIT" },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );

  await extension.unload();

  // Unblock pending requests on the server request handler side, so that the
  // test file can shutdown (otherwise the test run will be stuck after this
  // task completed).
  resolvePendingResponses();
});

add_task(async function test_truncated_string_properties() {
  const generateString = len => new Array(len).fill("a").join("");

  const LONG_STRINGS_ADDON_ID = "addon-with-long-strings-props@mochi.test";
  const { extension } = await installTestExtension({
    manifest: {
      name: generateString(400),
      description: generateString(400),
      applications: { gecko: { id: LONG_STRINGS_ADDON_ID } },
    },
  });

  // Override the test api server request handler, to be able to
  // intercept the properties actually submitted.
  let reportSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    reportSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  const report = await AbuseReporter.createAbuseReport(
    LONG_STRINGS_ADDON_ID,
    REPORT_OPTIONS
  );

  report.setMessage("fake-message");
  report.setReason("fake-reason");
  await report.submit();

  const expected = {
    addon_name: generateString(255),
    addon_summary: generateString(255),
  };

  Assert.deepEqual(
    {
      addon_name: reportSubmitted.addon_name,
      addon_summary: reportSubmitted.addon_summary,
    },
    expected,
    "Got the long strings truncated as expected"
  );

  await extension.unload();
});

add_task(async function test_report_recommended() {
  const NON_RECOMMENDED_ADDON_ID = "non-recommended-addon@mochi.test";
  const RECOMMENDED_ADDON_ID = "recommended-addon@mochi.test";

  const now = Date.now();
  const not_before = new Date(now - 3600000).toISOString();
  const not_after = new Date(now + 3600000).toISOString();

  const { extension: nonRecommended } = await installTestExtension({
    manifest: {
      name: "Fake non recommended addon",
      applications: { gecko: { id: NON_RECOMMENDED_ADDON_ID } },
    },
  });

  const { extension: recommended } = await installTestExtension({
    manifest: {
      name: "Fake recommended addon",
      applications: { gecko: { id: RECOMMENDED_ADDON_ID } },
    },
    files: {
      "mozilla-recommendation.json": {
        addon_id: RECOMMENDED_ADDON_ID,
        states: ["recommended"],
        validity: { not_before, not_after },
      },
    },
  });

  // Override the test api server request handler, to be able to
  // intercept the properties actually submitted.
  let reportSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    reportSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  async function checkReportedSignature(addonId, expectedAddonSignature) {
    clearAbuseReportState();
    const report = await AbuseReporter.createAbuseReport(
      addonId,
      REPORT_OPTIONS
    );
    report.setMessage("fake-message");
    report.setReason("fake-reason");
    await report.submit();
    equal(
      reportSubmitted.addon_signature,
      expectedAddonSignature,
      `Got the expected addon_signature for ${addonId}`
    );
  }

  await checkReportedSignature(NON_RECOMMENDED_ADDON_ID, "signed");
  await checkReportedSignature(RECOMMENDED_ADDON_ID, "curated");

  await nonRecommended.unload();
  await recommended.unload();
});

add_task(async function test_query_amo_details() {
  async function assertReportOnAMODetails({
    addonId,
    addonType = "extension",
    expectedReport,
  } = {}) {
    // Clear last report timestamp and any telemetry event recorded so far.
    clearAbuseReportState();
    Services.telemetry.clearEvents();

    const report = await AbuseReporter.createAbuseReport(addonId, {
      reportEntryPoint: "menu",
    });

    let reportSubmitted;
    apiRequestHandler = ({ data, request, response }) => {
      reportSubmitted = JSON.parse(data);
      handleSubmitRequest({ request, response });
    };

    report.setMessage("fake message");
    report.setReason("reason1");
    await report.submit();

    Assert.deepEqual(
      expectedReport,
      getProperties(reportSubmitted, Object.keys(expectedReport)),
      "Got the expected report properties"
    );

    // Telemetry recorded for the successfully submitted report.
    TelemetryTestUtils.assertEvents(
      [
        {
          object: "menu",
          value: addonId,
          extra: { addon_type: FAKE_AMO_DETAILS.type },
        },
      ],
      TELEMETRY_EVENTS_FILTERS
    );

    clearAbuseReportState();
  }

  // Add the expected AMO addons details.
  const addonId = "not-installed-addon@mochi.test";
  amoAddonDetailsMap.set(addonId, FAKE_AMO_DETAILS);

  // Test on the default en-US locale.
  Services.prefs.setCharPref(PREF_REQUIRED_LOCALE, "en-US");
  let locale = Services.locale.appLocaleAsBCP47;
  equal(locale, "en-US", "Got the expected app locale set");

  let expectedReport = {
    addon: addonId,
    addon_name: FAKE_AMO_DETAILS.name[locale],
    addon_version: FAKE_AMO_DETAILS.current_version.version,
    addon_install_source: "not_installed",
    addon_install_method: null,
    addon_signature: "curated",
  };

  await assertReportOnAMODetails({ addonId, expectedReport });

  // Test with a non-default locale also available in the AMO details.
  Services.prefs.setCharPref(PREF_REQUIRED_LOCALE, "it-IT");
  locale = Services.locale.appLocaleAsBCP47;
  equal(locale, "it-IT", "Got the expected app locale set");

  expectedReport = {
    ...expectedReport,
    addon_name: FAKE_AMO_DETAILS.name[locale],
  };
  await assertReportOnAMODetails({ addonId, expectedReport });

  // Test with a non-default locale not available in the AMO details.
  Services.prefs.setCharPref(PREF_REQUIRED_LOCALE, "fr-FR");
  locale = Services.locale.appLocaleAsBCP47;
  equal(locale, "fr-FR", "Got the expected app locale set");

  expectedReport = {
    ...expectedReport,
    // Fallbacks on en-US for non available locales.
    addon_name: FAKE_AMO_DETAILS.name["en-US"],
  };
  await assertReportOnAMODetails({ addonId, expectedReport });

  Services.prefs.clearUserPref(PREF_REQUIRED_LOCALE);

  amoAddonDetailsMap.clear();
});

add_task(async function test_statictheme_normalized_into_type_theme() {
  const themeId = "not-installed-statictheme@mochi.test";
  amoAddonDetailsMap.set(themeId, {
    ...FAKE_AMO_DETAILS,
    type: "statictheme",
  });

  const report = await AbuseReporter.createAbuseReport(themeId, REPORT_OPTIONS);

  equal(report.addon.id, themeId, "Got a report for the expected theme id");
  equal(report.addon.type, "theme", "Got the expected addon type");

  amoAddonDetailsMap.clear();
});
