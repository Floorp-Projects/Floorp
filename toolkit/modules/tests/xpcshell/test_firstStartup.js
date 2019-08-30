"use strict";

const { FirstStartup } = ChromeUtils.import(
  "resource://gre/modules/FirstStartup.jsm"
);

const PREF_TIMEOUT = "first-startup.timeout";
const PROBE_NAME = "firstStartup.statusCode";

add_task(async function test_success() {
  FirstStartup.init();
  equal(FirstStartup.state, FirstStartup.SUCCESS);

  const scalars = Services.telemetry.getSnapshotForScalars("main", false)
    .parent;
  ok(PROBE_NAME in scalars);
  equal(scalars[PROBE_NAME], FirstStartup.SUCCESS);
});

add_task(async function test_timeout() {
  Services.prefs.setIntPref(PREF_TIMEOUT, 0);
  FirstStartup.init();
  equal(FirstStartup.state, FirstStartup.TIMED_OUT);

  const scalars = Services.telemetry.getSnapshotForScalars("main", false)
    .parent;
  ok(PROBE_NAME in scalars);
  equal(scalars[PROBE_NAME], FirstStartup.TIMED_OUT);
});
