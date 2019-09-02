"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { FirstStartup } = ChromeUtils.import(
  "resource://gre/modules/FirstStartup.jsm"
);

const PREF_TIMEOUT = "first-startup.timeout";
const PROBE_NAME = "firstStartup.statusCode";

add_task(async function test_success() {
  FirstStartup.init();
  if (AppConstants.MOZ_NORMANDY) {
    equal(FirstStartup.state, FirstStartup.SUCCESS);
  } else {
    equal(FirstStartup.state, FirstStartup.UNSUPPORTED);
  }

  const scalars = Services.telemetry.getSnapshotForScalars("main", false)
    .parent;
  ok(PROBE_NAME in scalars);

  if (AppConstants.MOZ_NORMANDY) {
    equal(scalars[PROBE_NAME], FirstStartup.SUCCESS);
  } else {
    equal(scalars[PROBE_NAME], FirstStartup.UNSUPPORTED);
  }
});

add_task(async function test_timeout() {
  Services.prefs.setIntPref(PREF_TIMEOUT, 0);
  FirstStartup.init();

  if (AppConstants.MOZ_NORMANDY) {
    equal(FirstStartup.state, FirstStartup.TIMED_OUT);
  } else {
    equal(FirstStartup.state, FirstStartup.UNSUPPORTED);
  }

  const scalars = Services.telemetry.getSnapshotForScalars("main", false)
    .parent;
  ok(PROBE_NAME in scalars);
  if (AppConstants.MOZ_NORMANDY) {
    equal(scalars[PROBE_NAME], FirstStartup.TIMED_OUT);
  } else {
    equal(scalars[PROBE_NAME], FirstStartup.UNSUPPORTED);
  }
});
