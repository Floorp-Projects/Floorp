"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/AddonManager.jsm", this);
ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm", this);
ChromeUtils.import("resource://normandy/lib/ClientEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/PreferenceExperiments.jsm", this);


add_task(async function testTelemetry() {
  // setup
  await SpecialPowers.pushPrefEnv({set: [["privacy.reduceTimerPrecision", true]]});

  await TelemetryController.submitExternalPing("testfoo", {foo: 1});
  await TelemetryController.submitExternalPing("testbar", {bar: 2});
  await TelemetryController.submitExternalPing("testfoo", {foo: 3});

  // Test it can access telemetry
  const telemetry = await ClientEnvironment.telemetry;
  is(typeof telemetry, "object", "Telemetry is accesible");

  // Test it reads different types of telemetry
  is(telemetry.testfoo.payload.foo, 3, "telemetry filters pull the latest ping from a type");
  is(telemetry.testbar.payload.bar, 2, "telemetry filters pull from submitted telemetry pings");
});

add_task(async function testUserId() {
  // Test that userId is available
  ok(UUID_REGEX.test(ClientEnvironment.userId), "userId available");

  // test that it pulls from the right preference
  await SpecialPowers.pushPrefEnv({set: [["app.normandy.user_id", "fake id"]]});
  is(ClientEnvironment.userId, "fake id", "userId is pulled from preferences");
});

add_task(async function testDistribution() {
  // distribution id defaults to "default"
  is(ClientEnvironment.distribution, "default", "distribution has a default value");

  // distribution id is read from a preference
  await SpecialPowers.pushPrefEnv({set: [["distribution.id", "funnelcake"]]});
  is(ClientEnvironment.distribution, "funnelcake", "distribution is read from preferences");
});

const mockClassify = {country: "FR", request_time: new Date(2017, 1, 1)};
add_task(ClientEnvironment.withMockClassify(mockClassify, async function testCountryRequestTime() {
  // Test that country and request_time pull their data from the server.
  is(await ClientEnvironment.country, mockClassify.country, "country is read from the server API");
  is(
    await ClientEnvironment.request_time, mockClassify.request_time,
    "request_time is read from the server API"
  );
}));

add_task(async function testSync() {
  is(ClientEnvironment.syncMobileDevices, 0, "syncMobileDevices defaults to zero");
  is(ClientEnvironment.syncDesktopDevices, 0, "syncDesktopDevices defaults to zero");
  is(ClientEnvironment.syncTotalDevices, 0, "syncTotalDevices defaults to zero");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["services.sync.clients.devices.mobile", 5],
      ["services.sync.clients.devices.desktop", 4],
    ],
  });
  is(ClientEnvironment.syncMobileDevices, 5, "syncMobileDevices is read when set");
  is(ClientEnvironment.syncDesktopDevices, 4, "syncDesktopDevices is read when set");
  is(ClientEnvironment.syncTotalDevices, 9, "syncTotalDevices is read when set");
});

add_task(async function testDoNotTrack() {
  // doNotTrack defaults to false
  ok(!ClientEnvironment.doNotTrack, "doNotTrack has a default value");

  // doNotTrack is read from a preference
  await SpecialPowers.pushPrefEnv({set: [["privacy.donottrackheader.enabled", true]]});
  ok(ClientEnvironment.doNotTrack, "doNotTrack is read from preferences");
});

add_task(async function testExperiments() {
  const active = {name: "active", expired: false};
  const expired = {name: "expired", expired: true};
  const getAll = sinon.stub(PreferenceExperiments, "getAll", async () => [active, expired]);

  const experiments = await ClientEnvironment.experiments;
  Assert.deepEqual(
    experiments.all,
    ["active", "expired"],
    "experiments.all returns all stored experiment names",
  );
  Assert.deepEqual(
    experiments.active,
    ["active"],
    "experiments.active returns all active experiment names",
  );
  Assert.deepEqual(
    experiments.expired,
    ["expired"],
    "experiments.expired returns all expired experiment names",
  );

  getAll.restore();
});

add_task(withDriver(Assert, async function testAddonsInContext(driver) {
  // Create before install so that the listener is added before startup completes.
  const startupPromise = AddonTestUtils.promiseWebExtensionStartup("normandydriver@example.com");
  const addonId = await driver.addons.install(TEST_XPI_URL);
  await startupPromise;

  const addons = await ClientEnvironment.addons;
  Assert.deepEqual(addons[addonId], {
    id: [addonId],
    name: "normandy_fixture",
    version: "1.0",
    installDate: addons[addonId].installDate,
    isActive: true,
    type: "extension",
  }, "addons should be available in context");

  await driver.addons.uninstall(addonId);
}));

add_task(async function isFirstRun() {
  await SpecialPowers.pushPrefEnv({set: [["app.normandy.first_run", true]]});
  ok(ClientEnvironment.isFirstRun, "isFirstRun is read from preferences");
});
