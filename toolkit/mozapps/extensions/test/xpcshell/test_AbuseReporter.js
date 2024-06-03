/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AbuseReporter, AbuseReportError } = ChromeUtils.importESModule(
  "resource://gre/modules/AbuseReporter.sys.mjs"
);

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);

const APPVERSION = "1";
const ADDON_ID = "test-addon@tests.mozilla.org";
const FAKE_INSTALL_INFO = {
  source: "fake-Install:Source",
  method: "fake:install method",
};
const EXPECTED_API_RESPONSE = {
  id: ADDON_ID,
  some: "other-props",
};

async function installTestExtension(overrideOptions = {}) {
  const extOptions = {
    manifest: {
      browser_specific_settings: { gecko: { id: ADDON_ID } },
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
  equal(
    reportData.app,
    AppConstants.platform === "android" ? "android" : "firefox",
    "Got expected 'app'"
  );
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

async function assertRejectsAbuseReportError(promise, errorType, errorInfo) {
  let error;

  await Assert.rejects(
    promise,
    err => {
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

function handleSubmitRequest({ request, response }) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "application/json", false);
  response.write(JSON.stringify(EXPECTED_API_RESPONSE));
}

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

const server = createHttpServer({ hosts: ["test.addons.org"] });

// Mock abuse report API endpoint.
let apiRequestHandler;
server.registerPathHandler("/api/abuse/report/addon/", (request, response) => {
  const stream = request.bodyInputStream;
  const buffer = NetUtil.readInputStream(stream, stream.available());
  const data = new TextDecoder().decode(buffer);
  apiRequestHandler({ data, request, response });
});

add_setup(async () => {
  Services.prefs.setCharPref(
    "extensions.addonAbuseReport.url",
    "http://test.addons.org/api/abuse/report/addon/"
  );

  await promiseStartupManager();
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

add_task(async function test_sendAbuseReport() {
  const { addon, extension } = await installTestExtension();
  // Data passed by the caller.
  const formData = { "some-data-from-the-caller": true };
  // Metadata stored by Gecko, only passed when the add-on is installed, which
  // is what this test case verifies.
  //
  // NOTE: We JSON stringify + parse to get rid of the undefined values, which
  // we do not send to the server.
  const metadata = JSON.parse(
    JSON.stringify(await AbuseReporter.getReportData(addon))
  );
  // Register a request handler to (1) access the data submitted and (2) return
  // a 200 response.
  let dataSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    Assert.equal(
      request.getHeader("content-type"),
      "application/json",
      "expected content-type header"
    );
    Assert.ok(
      !request.hasHeader("authorization"),
      "expected no authorization header"
    );

    dataSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  const response = await AbuseReporter.sendAbuseReport(ADDON_ID, formData);

  Assert.deepEqual(
    response,
    EXPECTED_API_RESPONSE,
    "expected successful response"
  );
  Assert.deepEqual(
    dataSubmitted,
    {
      ...formData,
      ...metadata,
      // The add-on ID is unconditionally passed as `addon` on purpose. See:
      // https://mozilla.github.io/addons-server/topics/api/abuse.html#submitting-an-add-on-abuse-report
      addon: ADDON_ID,
    },
    "expected the right data to be sent to the server"
  );

  await extension.unload();
});

add_task(async function test_sendAbuseReport_addon_not_installed() {
  const formData = { "some-data-from-the-caller": true };
  // Register a request handler to (1) access the data submitted and (2) return
  // a 200 response.
  let dataSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    Assert.equal(
      request.getHeader("content-type"),
      "application/json",
      "expected content-type header"
    );
    Assert.ok(
      !request.hasHeader("authorization"),
      "expected no authorization header"
    );

    dataSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  const response = await AbuseReporter.sendAbuseReport(ADDON_ID, formData);

  Assert.deepEqual(
    response,
    EXPECTED_API_RESPONSE,
    "expected successful response"
  );
  Assert.deepEqual(
    dataSubmitted,
    {
      ...formData,
      // The add-on ID is unconditionally passed as `addon` on purpose. See:
      // https://mozilla.github.io/addons-server/topics/api/abuse.html#submitting-an-add-on-abuse-report
      addon: ADDON_ID,
    },
    "expected the right data to be sent to the server"
  );
});

add_task(async function test_sendAbuseReport_with_authorization() {
  const { addon, extension } = await installTestExtension();
  // Data passed by the caller.
  const formData = { "some-data-from-the-caller": true };
  // Metadata stored by Gecko, only passed when the add-on is installed, which
  // is what this test case verifies.
  //
  // NOTE: We JSON stringify + parse to get rid of the undefined values, which
  // we do not send to the server.
  const metadata = JSON.parse(
    JSON.stringify(await AbuseReporter.getReportData(addon))
  );
  const authorization = "some authorization header";
  // Register a request handler to (1) access the data submitted and (2) return
  // a 200 response.
  let dataSubmitted;
  apiRequestHandler = ({ data, request, response }) => {
    Assert.equal(
      request.getHeader("content-type"),
      "application/json",
      "expected content-type header"
    );
    Assert.equal(
      request.getHeader("authorization"),
      authorization,
      "expected authorization header"
    );

    dataSubmitted = JSON.parse(data);
    handleSubmitRequest({ request, response });
  };

  const response = await AbuseReporter.sendAbuseReport(ADDON_ID, formData, {
    authorization,
  });

  Assert.deepEqual(
    response,
    EXPECTED_API_RESPONSE,
    "expected successful response"
  );
  Assert.deepEqual(
    dataSubmitted,
    {
      ...formData,
      ...metadata,
      // The add-on ID is unconditionally passed as `addon` on purpose. See:
      // https://mozilla.github.io/addons-server/topics/api/abuse.html#submitting-an-add-on-abuse-report
      addon: ADDON_ID,
    },
    "expected the right data to be sent to the server"
  );

  await extension.unload();
});

add_task(async function test_sendAbuseReport_errors() {
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

    let requestReceived = false;
    apiRequestHandler = ({ request, response }) => {
      requestReceived = true;
      response.setStatusLine(request.httpVersion, responseStatus, "Error");
      response.write(responseText);
    };

    const promise = AbuseReporter.sendAbuseReport(ADDON_ID, {});
    if (typeof expectedErrorType === "string") {
      // Assert a specific AbuseReportError errorType.
      await assertRejectsAbuseReportError(
        promise,
        expectedErrorType,
        expectedErrorInfo
      );
    } else {
      // Assert on a given Error class.
      await Assert.rejects(
        promise,
        expectedErrorType,
        "expected correct Error class"
      );
    }
    equal(
      requestReceived,
      expectRequest,
      `${expectRequest ? "" : "Not "}received a request as expected`
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
    "extensions.addonAbuseReport.url",
    "invalid-protocol://abuse-report"
  );
  await testErrorCode({
    expectedErrorType: "ERROR_NETWORK",
    expectRequest: false,
  });

  await extension.unload();
});
