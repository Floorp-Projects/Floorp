/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  ClientEnvironmentBase:
    "resource://gre/modules/components-utils/ClientEnvironment.sys.mjs",
  NormandyTestUtils: "resource://testing-common/NormandyTestUtils.sys.mjs",
  TelemetryController: "resource://gre/modules/TelemetryController.sys.mjs",
  updateAppInfo: "resource://testing-common/AppInfo.sys.mjs",
});

add_setup(() => {
  updateAppInfo();
});

add_task(async function test_OS_data() {
  const os = ClientEnvironmentBase.os;
  ok(os !== undefined, "OS data should be available in the context");

  let osCount = 0;
  if (os.isWindows) {
    osCount += 1;
  }
  if (os.isMac) {
    osCount += 1;
  }
  if (os.isLinux) {
    osCount += 1;
  }
  ok(osCount <= 1, "At most one OS should match");

  // if on Windows, Windows versions should be set, and Mac versions should not be
  if (os.isWindows) {
    equal(
      typeof os.windowsVersion,
      "number",
      "Windows version should be a number"
    );
    equal(
      typeof os.windowsBuildNumber,
      "number",
      "Windows build number should be a number"
    );
    equal(os.macVersion, null, "Mac version should not be set");
    equal(os.darwinVersion, null, "Darwin version should not be set");
  }

  // if on Mac, Mac versions should be set, and Windows versions should not be
  if (os.isMac) {
    equal(typeof os.macVersion, "number", "Mac version should be a number");
    equal(
      typeof os.darwinVersion,
      "number",
      "Darwin version should be a number"
    );
    equal(os.windowsVersion, null, "Windows version should not be set");
    equal(
      os.windowsBuildNumber,
      null,
      "Windows build number version should not be set"
    );
  }

  // if on Linux, no versions should be set
  if (os.isLinux) {
    equal(os.macVersion, null, "Mac version should not be set");
    equal(os.darwinVersion, null, "Darwin version should not be set");
    equal(os.windowsVersion, null, "Windows version should not be set");
    equal(
      os.windowsBuildNumber,
      null,
      "Windows build number version should not be set"
    );
  }
});

add_task(async function test_attributionData() {
  try {
    await ClientEnvironmentBase.attribution;
  } catch (ex) {
    equal(
      ex.result,
      Cr.NS_ERROR_FILE_NOT_FOUND,
      "Test environment does not have attribution data"
    );
  }
});

add_task(async function testLiveTelemetry() {
  // Setup telemetry so we can read from it
  do_get_profile(true);
  await TelemetryController.testSetup();

  equal(
    ClientEnvironmentBase.liveTelemetry.main.environment.build.displayVersion,
    AppConstants.MOZ_APP_VERSION_DISPLAY,
    "Telemetry data is available"
  );

  Assert.throws(
    () => ClientEnvironmentBase.liveTelemetry.anotherPingType,
    /Live telemetry.*anotherPingType/,
    "Non-main pings should raise an error if accessed"
  );

  // Put things back the way we found them
  await TelemetryController.testShutdown();
});

add_task(function testBuildId() {
  ok(
    ClientEnvironmentBase.appinfo !== undefined,
    "appinfo should be available in the context"
  );
  ok(
    typeof ClientEnvironmentBase.appinfo === "object",
    "appinfo should be an object"
  );
  ok(
    typeof ClientEnvironmentBase.appinfo.appBuildID === "string",
    "buildId should be a string"
  );
});

add_task(
  {
    skip_if: () => AppConstants.MOZ_BUILD_APP != "browser",
  },
  async function testRandomizationId() {
    // Should generate an id if none is set
    await Services.prefs.clearUserPref("app.normandy.user_id");
    Assert.ok(
      NormandyTestUtils.isUuid(ClientEnvironmentBase.randomizationId),
      "randomizationId should be available"
    );

    // Should read the right preference
    await Services.prefs.setStringPref("app.normandy.user_id", "fake id");
    Assert.equal(
      ClientEnvironmentBase.randomizationId,
      "fake id",
      "randomizationId should read from preferences"
    );
  }
);
