"use strict";

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);

const { AddonRollouts } = ChromeUtils.importESModule(
  "resource://normandy/lib/AddonRollouts.sys.mjs"
);
const { ClientEnvironment } = ChromeUtils.importESModule(
  "resource://normandy/lib/ClientEnvironment.sys.mjs"
);
const { PreferenceExperiments } = ChromeUtils.importESModule(
  "resource://normandy/lib/PreferenceExperiments.sys.mjs"
);
const { PreferenceRollouts } = ChromeUtils.importESModule(
  "resource://normandy/lib/PreferenceRollouts.sys.mjs"
);
const { RecipeRunner } = ChromeUtils.importESModule(
  "resource://normandy/lib/RecipeRunner.sys.mjs"
);
const { NormandyTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/NormandyTestUtils.sys.mjs"
);

add_task(async function testTelemetry() {
  // setup
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.reduceTimerPrecision", true]],
  });

  await TelemetryController.submitExternalPing("testfoo", { foo: 1 });
  await TelemetryController.submitExternalPing("testbar", { bar: 2 });
  await TelemetryController.submitExternalPing("testfoo", { foo: 3 });

  // Test it can access telemetry
  const telemetry = await ClientEnvironment.telemetry;
  is(typeof telemetry, "object", "Telemetry is accesible");

  // Test it reads different types of telemetry
  is(
    telemetry.testfoo.payload.foo,
    3,
    "telemetry filters pull the latest ping from a type"
  );
  is(
    telemetry.testbar.payload.bar,
    2,
    "telemetry filters pull from submitted telemetry pings"
  );
});

add_task(async function testUserId() {
  // Test that userId is available
  ok(NormandyTestUtils.isUuid(ClientEnvironment.userId), "userId available");

  // test that it pulls from the right preference
  await SpecialPowers.pushPrefEnv({
    set: [["app.normandy.user_id", "fake id"]],
  });
  is(ClientEnvironment.userId, "fake id", "userId is pulled from preferences");
});

add_task(async function testDistribution() {
  // distribution id defaults to "default" for most builds, and
  // "mozilla-MSIX" for MSIX builds.
  is(
    ClientEnvironment.distribution,
    AppConstants.platform === "win" &&
      Services.sysinfo.getProperty("hasWinPackageId")
      ? "mozilla-MSIX"
      : "default",
    "distribution has a default value"
  );

  // distribution id is read from a preference
  Services.prefs
    .getDefaultBranch(null)
    .setStringPref("distribution.id", "funnelcake");
  is(
    ClientEnvironment.distribution,
    "funnelcake",
    "distribution is read from preferences"
  );
  Services.prefs
    .getDefaultBranch(null)
    .setStringPref("distribution.id", "default");
});

const mockClassify = { country: "FR", request_time: new Date(2017, 1, 1) };
add_task(
  ClientEnvironment.withMockClassify(
    mockClassify,
    async function testCountryRequestTime() {
      // Test that country and request_time pull their data from the server.
      is(
        await ClientEnvironment.country,
        mockClassify.country,
        "country is read from the server API"
      );
      is(
        await ClientEnvironment.request_time,
        mockClassify.request_time,
        "request_time is read from the server API"
      );
    }
  )
);

add_task(async function testSync() {
  is(
    ClientEnvironment.syncMobileDevices,
    0,
    "syncMobileDevices defaults to zero"
  );
  is(
    ClientEnvironment.syncDesktopDevices,
    0,
    "syncDesktopDevices defaults to zero"
  );
  is(
    ClientEnvironment.syncTotalDevices,
    0,
    "syncTotalDevices defaults to zero"
  );
  await SpecialPowers.pushPrefEnv({
    set: [
      ["services.sync.clients.devices.mobile", 5],
      ["services.sync.clients.devices.desktop", 4],
    ],
  });
  is(
    ClientEnvironment.syncMobileDevices,
    5,
    "syncMobileDevices is read when set"
  );
  is(
    ClientEnvironment.syncDesktopDevices,
    4,
    "syncDesktopDevices is read when set"
  );
  is(
    ClientEnvironment.syncTotalDevices,
    9,
    "syncTotalDevices is read when set"
  );
});

add_task(async function testDoNotTrack() {
  // doNotTrack defaults to false
  ok(!ClientEnvironment.doNotTrack, "doNotTrack has a default value");

  // doNotTrack is read from a preference
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.donottrackheader.enabled", true]],
  });
  ok(ClientEnvironment.doNotTrack, "doNotTrack is read from preferences");
});

add_task(async function testExperiments() {
  const active = { slug: "active", expired: false };
  const expired = { slug: "expired", expired: true };
  const getAll = sinon
    .stub(PreferenceExperiments, "getAll")
    .callsFake(async () => [active, expired]);

  const experiments = await ClientEnvironment.experiments;
  Assert.deepEqual(
    experiments.all,
    ["active", "expired"],
    "experiments.all returns all stored experiment names"
  );
  Assert.deepEqual(
    experiments.active,
    ["active"],
    "experiments.active returns all active experiment names"
  );
  Assert.deepEqual(
    experiments.expired,
    ["expired"],
    "experiments.expired returns all expired experiment names"
  );

  getAll.restore();
});

add_task(async function isFirstRun() {
  await SpecialPowers.pushPrefEnv({ set: [["app.normandy.first_run", true]] });
  ok(ClientEnvironment.isFirstRun, "isFirstRun is read from preferences");
});

decorate_task(
  PreferenceExperiments.withMockExperiments([
    NormandyTestUtils.factories.preferenceStudyFactory({
      branch: "a-test-branch",
    }),
  ]),
  AddonStudies.withStudies([
    NormandyTestUtils.factories.branchedAddonStudyFactory({
      branch: "b-test-branch",
    }),
  ]),
  async function testStudies({
    prefExperiments: [prefExperiment],
    addonStudies: [addonStudy],
  }) {
    Assert.deepEqual(
      await ClientEnvironment.studies,
      {
        pref: {
          [prefExperiment.slug]: prefExperiment,
        },
        addon: {
          [addonStudy.slug]: addonStudy,
        },
      },
      "addon and preference studies shold be accessible"
    );
    is(
      (await ClientEnvironment.studies).pref[prefExperiment.slug].branch,
      "a-test-branch",
      "A specific preference experiment field should be accessible in the context"
    );
    is(
      (await ClientEnvironment.studies).addon[addonStudy.slug].branch,
      "b-test-branch",
      "A specific addon study field should be accessible in the context"
    );

    ok(RecipeRunner.getCapabilities().has("jexl.context.normandy.studies"));
    ok(RecipeRunner.getCapabilities().has("jexl.context.env.studies"));
  }
);

decorate_task(PreferenceRollouts.withTestMock(), async function testRollouts() {
  const prefRollout = {
    slug: "test-rollout",
    preference: [],
  };
  await PreferenceRollouts.add(prefRollout);
  const addonRollout = {
    slug: "test-rollout-1",
    extension: {},
  };
  await AddonRollouts.add(addonRollout);

  Assert.deepEqual(
    await ClientEnvironment.rollouts,
    {
      pref: {
        [prefRollout.slug]: prefRollout,
      },
      addon: {
        [addonRollout.slug]: addonRollout,
      },
    },
    "addon and preference rollouts should be accessible"
  );
  is(
    (await ClientEnvironment.rollouts).pref[prefRollout.slug].slug,
    prefRollout.slug,
    "A specific preference rollout field should be accessible in the context"
  );
  is(
    (await ClientEnvironment.rollouts).addon[addonRollout.slug].slug,
    addonRollout.slug,
    "A specific addon rollout field should be accessible in the context"
  );

  ok(RecipeRunner.getCapabilities().has("jexl.context.normandy.rollouts"));
  ok(RecipeRunner.getCapabilities().has("jexl.context.env.rollouts"));
});
