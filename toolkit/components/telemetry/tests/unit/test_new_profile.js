/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { BrowserUsageTelemetry } = ChromeUtils.importESModule(
  "resource:///modules/BrowserUsageTelemetry.sys.mjs"
);
const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);

const TIMESTAMP_TEST_VALUE = "007357735773577357";

let jsonPath = Services.dirsvc.get("XREExeF", Ci.nsIFile).parent;
jsonPath.append("installation_telemetry.json");

let scalarSnapshot = null;

let testDataJSON = null;
if (Services.sysinfo.getProperty("hasWinPackageId")) {
  // On MSIX the test data is only used for
  // verification purposes, it isn't written anywhere.
  testDataJSON = {
    installer_type: "msix",
    version: AppConstants.MOZ_APP_VERSION,
    build_id: AppConstants.MOZ_BULIDID,
    admin_user: false,
    // The undefined fields are not recorded on MSIX.
    silent: undefined,
    from_msi: undefined,
    default_path: undefined,
    install_existed: false,
    other_inst: false,
    other_msix_inst: false,
    profdir_existed: false,
  };
} else {
  testDataJSON = {
    version: "000.0a1",
    build_id: "00000000000000",
    admin_user: true,
    install_existed: false,
    profdir_existed: false,
    installer_type: "full",
    other_inst: false,
    other_msix_inst: false,
    silent: false,
    from_msi: false,
    default_path: true,
    install_timestamp: TIMESTAMP_TEST_VALUE,
  };
}

async function createTestInstallationTelemetry() {
  // Only write the file on non-MSIX
  if (Services.sysinfo.getProperty("hasWinPackageId")) {
    return;
  }
  let utf16Array = new Uint16Array(
    [...JSON.stringify(testDataJSON)].map(char => char.charCodeAt(0))
  );
  let byteArray = new Uint8Array(utf16Array.buffer);
  await IOUtils.write(jsonPath.path, byteArray);
  registerCleanupFunction(async () => {
    await IOUtils.remove(jsonPath.path, { ignoreAbsent: true });
  });
}

add_setup(
  {
    skip_if: () =>
      Services.prefs.getBoolPref("telemetry.fog.artifact_build", false),
  },
  function () {
    do_get_profile();
    Services.fog.initializeFOG();
  }
);

add_setup(async function () {
  Services.telemetry.clearScalars();
  await createTestInstallationTelemetry();
  await BrowserUsageTelemetry.reportInstallationTelemetry();
  scalarSnapshot = Services.telemetry.getSnapshotForScalars(
    "new-profile",
    false
  ).parent;
});

add_task(async function test_new_profile_ping() {
  Object.entries(testDataJSON).forEach(([key, value]) => {
    // We don't log "build_id" as a scalar
    if (key == "build_id") {
      return;
    }
    // We must not log install_timestamp as a scalar due
    // to fingerprinting risk. Verify that we don't.
    if (key == "install_timestamp") {
      Assert.ok(
        !(`installation.firstSeen.${key}` in scalarSnapshot),
        "Must not record " + key + " as a scalar"
      );
      return;
    }
    TelemetryTestUtils.assertScalar(
      scalarSnapshot,
      `installation.firstSeen.${key}`,
      value
    );
  });
});

add_task(
  {
    skip_if: () =>
      Services.prefs.getBoolPref("telemetry.fog.artifact_build", false),
  },
  async function test_new_profile_gifft_mirror() {
    Assert.equal(
      testDataJSON.installer_type,
      Glean.installationFirstSeen.installerType.testGetValue()
    );
    Assert.equal(
      testDataJSON.version,
      Glean.installationFirstSeen.version.testGetValue()
    );
    Assert.equal(
      testDataJSON.admin_user,
      Glean.installationFirstSeen.adminUser.testGetValue()
    );
    Assert.equal(
      testDataJSON.install_existed,
      Glean.installationFirstSeen.installExisted.testGetValue()
    );
    Assert.equal(
      testDataJSON.profdir_existed,
      Glean.installationFirstSeen.profdirExisted.testGetValue()
    );
    Assert.equal(
      testDataJSON.other_inst,
      Glean.installationFirstSeen.otherInst.testGetValue()
    );
    Assert.equal(
      testDataJSON.other_msix_inst,
      Glean.installationFirstSeen.otherMsixInst.testGetValue()
    );
    // These fields are only recorded on the full installer.
    if (!Services.sysinfo.getProperty("hasWinPackageId")) {
      Assert.equal(
        testDataJSON.silent,
        Glean.installationFirstSeen.silent.testGetValue()
      );
      Assert.equal(
        testDataJSON.from_msi,
        Glean.installationFirstSeen.fromMsi.testGetValue()
      );
      Assert.equal(
        testDataJSON.default_path,
        Glean.installationFirstSeen.defaultPath.testGetValue()
      );
    }
  }
);
