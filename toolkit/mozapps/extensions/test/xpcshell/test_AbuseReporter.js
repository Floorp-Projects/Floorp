/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { AbuseReporter } = ChromeUtils.importESModule(
  "resource://gre/modules/AbuseReporter.sys.mjs"
);

const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);

const APPNAME = "XPCShell";
const APPVERSION = "1";
const ADDON_ID = "test-addon@tests.mozilla.org";
const FAKE_INSTALL_INFO = {
  source: "fake-Install:Source",
  method: "fake:install method",
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

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "49");

add_setup(async () => {
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
