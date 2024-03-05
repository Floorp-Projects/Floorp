"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { FirstStartup } = ChromeUtils.importESModule(
  "resource://gre/modules/FirstStartup.sys.mjs"
);
const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

const PREF_TIMEOUT = "first-startup.timeout";

add_setup(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // FOG needs to be initialized in order for data to flow.
  Services.fog.initializeFOG();
});

add_task(async function test_success() {
  updateAppInfo();

  let submissionPromise;
  FirstStartup.resetForTesting();

  if (AppConstants.MOZ_NORMANDY || AppConstants.MOZ_UPDATE_AGENT) {
    submissionPromise = new Promise(resolve => {
      GleanPings.firstStartup.testBeforeNextSubmit(() => {
        Assert.equal(FirstStartup.state, FirstStartup.SUCCESS);
        Assert.ok(Glean.firstStartup.newProfile.testGetValue());
        Assert.equal(
          Glean.firstStartup.statusCode.testGetValue(),
          FirstStartup.SUCCESS
        );

        if (AppConstants.MOZ_NORMANDY) {
          Assert.ok(Glean.firstStartup.normandyInitTime.testGetValue() > 0);
        }

        if (AppConstants.MOZ_UPDATE_AGENT) {
          Assert.ok(Glean.firstStartup.deleteTasksTime.testGetValue() > 0);
        }

        resolve();
      });
    });
  } else {
    submissionPromise = new Promise(resolve => {
      GleanPings.firstStartup.testBeforeNextSubmit(() => {
        Assert.equal(FirstStartup.state, FirstStartup.UNSUPPORTED);
        Assert.ok(Glean.firstStartup.newProfile.testGetValue());
        Assert.equal(
          Glean.firstStartup.statusCode.testGetValue(),
          FirstStartup.UNSUPPORTED
        );
        resolve();
      });
    });
  }

  FirstStartup.init(true /* newProfile */);
  await submissionPromise;
});

add_task(async function test_timeout() {
  updateAppInfo();
  Services.prefs.setIntPref(PREF_TIMEOUT, 0);
  FirstStartup.resetForTesting();

  let submissionPromise;

  if (AppConstants.MOZ_NORMANDY || AppConstants.MOZ_UPDATE_AGENT) {
    submissionPromise = new Promise(resolve => {
      GleanPings.firstStartup.testBeforeNextSubmit(() => {
        Assert.equal(FirstStartup.state, FirstStartup.TIMED_OUT);
        Assert.ok(Glean.firstStartup.elapsed.testGetValue() > 0);
        Assert.ok(Glean.firstStartup.newProfile.testGetValue());

        if (AppConstants.MOZ_NORMANDY) {
          Assert.ok(Glean.firstStartup.normandyInitTime.testGetValue() > 0);
        }

        if (AppConstants.MOZ_UPDATE_AGENT) {
          Assert.ok(Glean.firstStartup.deleteTasksTime.testGetValue() > 0);
        }

        resolve();
      });
    });
  } else {
    submissionPromise = new Promise(resolve => {
      GleanPings.firstStartup.testBeforeNextSubmit(() => {
        Assert.equal(FirstStartup.state, FirstStartup.UNSUPPORTED);
        Assert.equal(Glean.firstStartup.elapsed.testGetValue(), 0);
        Assert.ok(Glean.firstStartup.newProfile.testGetValue());
        resolve();
      });
    });
  }

  FirstStartup.init(true /* newProfile */);
  await submissionPromise;
});

add_task(async function test_existing_profile() {
  FirstStartup.resetForTesting();

  let submissionPromise = new Promise(resolve => {
    GleanPings.firstStartup.testBeforeNextSubmit(() => {
      Assert.equal(FirstStartup.state, FirstStartup.NOT_STARTED);
      Assert.ok(!Glean.firstStartup.newProfile.testGetValue());
      resolve();
    });
  });

  FirstStartup.init(false /* newProfile */);
  await submissionPromise;
});
