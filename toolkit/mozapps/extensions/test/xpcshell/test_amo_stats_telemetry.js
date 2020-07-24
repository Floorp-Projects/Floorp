/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "42"
);

add_task(async function setup() {
  // We need to set this pref to `true` in order to collect add-ons Telemetry
  // data (which is considered extended data and disabled in CI).
  const overridePreReleasePref = "toolkit.telemetry.testing.overridePreRelease";
  let oldOverrideValue = Services.prefs.getBoolPref(
    overridePreReleasePref,
    false
  );
  Services.prefs.setBoolPref(overridePreReleasePref, true);
  registerCleanupFunction(() => {
    Services.prefs.setBoolPref(overridePreReleasePref, oldOverrideValue);
  });

  await TelemetryController.testSetup();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_ping_payload_and_environment() {
  const extensions = [
    {
      id: "addons-telemetry@test-extension-1",
      name: "some extension 1",
      version: "1.2.3",
    },
    {
      id: "addons-telemetry@test-extension-2",
      name: "some extension 2",
      version: "0.1",
    },
  ];

  // Install some extensions.
  const installedExtensions = [];
  for (const { id, name, version } of extensions) {
    const extension = ExtensionTestUtils.loadExtension({
      manifest: {
        name,
        version,
        applications: { gecko: { id } },
      },
      useAddonManager: "permanent",
    });
    installedExtensions.push(extension);

    await extension.startup();
  }

  const { payload, environment } = TelemetryController.getCurrentPingData();

  // Important: `payload.info.addons` is being used for AMO usage stats.
  Assert.ok("addons" in payload.info, "payload.info.addons is defined");
  Assert.equal(
    payload.info.addons,
    extensions
      .map(({ id, version }) => `${encodeURIComponent(id)}:${version}`)
      .join(",")
  );
  Assert.ok(
    "XPI" in payload.addonDetails,
    "payload.addonDetails.XPI is defined"
  );
  for (const { id, name } of extensions) {
    Assert.ok(id in payload.addonDetails.XPI);
    Assert.equal(payload.addonDetails.XPI[id].name, name);
  }

  const { addons } = environment;
  Assert.ok(
    "activeAddons" in addons,
    "environment.addons.activeAddons is defined"
  );
  Assert.ok("theme" in addons, "environment.addons.theme is defined");
  for (const { id } of extensions) {
    Assert.ok(id in environment.addons.activeAddons);
  }

  for (const extension of installedExtensions) {
    await extension.unload();
  }
});

add_task(async function cleanup() {
  await TelemetryController.testShutdown();
});
