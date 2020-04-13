/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { ClientEnvironmentBase } = ChromeUtils.import(
  "resource://gre/modules/components-utils/ClientEnvironment.jsm"
);
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// OS Data
add_task(async () => {
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

add_task(async () => {
  try {
    await ClientEnvironmentBase.attribution;
  } catch (ex) {
    equal(
      ex.name,
      "NS_ERROR_FILE_NOT_FOUND",
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
