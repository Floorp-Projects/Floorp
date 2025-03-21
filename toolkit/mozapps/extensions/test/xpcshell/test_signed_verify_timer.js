/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Enable signature checks for these tests
gUseRealCertChecks = true;
ExtensionTestUtils.mockAppInfo();

// Same as XPIProvider.sys.mjs.
const XPI_SIGNATURE_CHECK_PERIOD = 24 * 60 * 60;
const PREF_LAST_TIME = "app.update.lastUpdateTime.xpi-signature-verification";
const PREF_LAST_SIGNATURE_CHECKPOINT = "extensions.signatureCheckpoint";

// Add/subtract a bit of time when testing time-related stuff, to avoid
// intermittent test failures if the clock somehow skews.
const CLOCK_SKEW = 2;

function nowSecs() {
  return Math.floor(Date.now() / 1000);
}

const gUTM = Cc["@mozilla.org/updates/timer-manager;1"].getService(
  Ci.nsIUpdateTimerManager
);

function skewTimestampForVisibilityInTest() {
  // The test repeatedly checks whether the time has been updated (to "now"),
  // and can only see the difference if there is a difference between the
  // previous timestamp and "now". Therefore we tweak the timestamp.
  let time = Services.prefs.getIntPref(PREF_LAST_TIME);
  time -= 1;
  Services.prefs.setIntPref(PREF_LAST_TIME, time);
  return time;
}

add_setup(async () => {
  // Enable logging of UpdateTimerManager.sys.mjs for easier debugging.
  Services.prefs.setBoolPref("app.update.log", true);

  // Pretend that every timer has already fired very recently. Otherwise all
  // other random kinds of activities are triggered on utm-test-init, while we
  // are only interested in the xpi-signature-verification timer.
  for (const { value } of Services.catMan.enumerateCategory("update-timer")) {
    const timerID = value.split(",")[2];
    Services.prefs.setIntPref(
      `app.update.lastUpdateTime.${timerID}`,
      nowSecs() - CLOCK_SKEW
    );
  }

  // The internal repeating timer is fired as frequently as the most frequently
  // scheduled timer. Set a 1 second timer so that the test waits for at most
  // 1 second for a timer to fire, instead of e.g. 30 seconds.
  Services.prefs.setIntPref("app.update.timerMinimumDelay", 1);
  Services.prefs.setIntPref("app.update.timerFirstInterval", 1000);
  gUTM.registerTimer("test-dummy-ensure-quick-progress", () => {}, 1);

  AddonTestUtils.on("addon-manager-shutdown", () => {
    // XPIProvider registers a timer at its startup(). That timer is never
    // unregistered anywhere, so we do it here.
    gUTM.unregisterTimer("xpi-signature-verification");
  });

  // Now initialize the timer, so that the timer registered by XPIProvider's
  // startup() will actually do something.
  gUTM.QueryInterface(Ci.nsIObserver).observe(null, "utm-test-init", "");
});

add_task(useAMOStageCert(), async function test_scheduled_signature_checks() {
  // This test checks whether signature verification is scheduled when expected,
  // where the following states are relevant:
  // - Whether this is a new or existing profile.
  // - Value of PREF_LAST_TIME.
  // - Value of PREF_LAST_SIGNATURE_CHECKPOINT.
  // - Signed state of add-on.
  // - Value of xpinstall.signatures.dev-root pref (to simulate invalid cert).
  //
  // There are two potential triggers of signature check:
  // - Previous check was too long ago (according to PREF_LAST_TIME).
  // - Browser "updated" bumped checkpoint (PREF_LAST_SIGNATURE_CHECKPOINT).
  //
  // Each significant state transition in this test starts with:
  // info("Simulating ... ");

  // Pretend that we have already ran the update timer relatively recently,
  // so that the timer does not fire unexpectedly for unrelated reasons.
  const FIRST_RUN_VERIFICATION_TIME = nowSecs() - CLOCK_SKEW;
  Services.prefs.setIntPref(PREF_LAST_TIME, FIRST_RUN_VERIFICATION_TIME);

  info("Simulating first run with new profile");
  Assert.ok(
    !Services.prefs.prefHasUserValue(PREF_LAST_SIGNATURE_CHECKPOINT),
    "Checkpoint not set before first run"
  );

  await promiseStartupManager();

  Assert.ok(
    Services.prefs.prefHasUserValue(PREF_LAST_SIGNATURE_CHECKPOINT),
    "Checkpoint set after first run"
  );

  Assert.equal(
    Services.prefs.getIntPref(PREF_LAST_TIME),
    FIRST_RUN_VERIFICATION_TIME,
    "On startup of new profile, checkpoint is set without forced verification"
  );

  await promiseInstallFile(do_get_file("data/signing_checks/signed1.xpi"));
  async function getSignedState() {
    // The addon ID is of signed1.xpi.
    let addon = await promiseAddonByID("test@somewhere.com");
    return addon.signedState;
  }
  Assert.equal(
    await getSignedState(),
    AddonManager.SIGNEDSTATE_SIGNED,
    "Signature correct after install"
  );

  await promiseRestartManager();
  Assert.equal(
    Services.prefs.getIntPref(PREF_LAST_TIME),
    FIRST_RUN_VERIFICATION_TIME,
    "Signature verification not scheduled yet"
  );

  info("Simulating that the add-on should be considered signed incorrectly");
  // Toggle the root cert, which would invalidate the signature.
  Services.prefs.setBoolPref("xpinstall.signatures.dev-root", false);

  await promiseRestartManager();
  Assert.equal(
    Services.prefs.getIntPref(PREF_LAST_TIME),
    FIRST_RUN_VERIFICATION_TIME,
    "Signature verification not scheduled yet"
  );
  Assert.equal(
    await getSignedState(),
    AddonManager.SIGNEDSTATE_SIGNED,
    "Signature still correct despite different root (not verified yet)"
  );

  await promiseShutdownManager();

  info("Simulating that update check has happened too long ago");
  const TOO_LONG_AGO_TIME = nowSecs() - XPI_SIGNATURE_CHECK_PERIOD - CLOCK_SKEW;
  Services.prefs.setIntPref(PREF_LAST_TIME, TOO_LONG_AGO_TIME);

  let verifiedPromise = TestUtils.topicObserved("xpi-signature-changed");
  await promiseStartupManager();
  Assert.notEqual(
    Services.prefs.getIntPref(PREF_LAST_TIME),
    FIRST_RUN_VERIFICATION_TIME,
    "Signature verification happens when the last check was too long ago"
  );
  await verifiedPromise;
  Assert.equal(
    await getSignedState(),
    AddonManager.SIGNEDSTATE_UNKNOWN,
    "Detected signature invalidation (due to bad root) after verification"
  );

  const LAST_TIMER_TRIGGERED_TIME = skewTimestampForVisibilityInTest();

  info("Simulating that the add-on should be considered signed correctly");
  // Toggle the root cert, which makes the signature valid again.
  Services.prefs.setBoolPref("xpinstall.signatures.dev-root", true);
  await promiseRestartManager();
  Assert.equal(
    Services.prefs.getIntPref(PREF_LAST_TIME),
    LAST_TIMER_TRIGGERED_TIME,
    "Signature verification skipped because it happened recently"
  );
  Assert.equal(
    await getSignedState(),
    AddonManager.SIGNEDSTATE_UNKNOWN,
    "Signature out of date because verification was skipped"
  );
  await promiseShutdownManager();

  // Regression test for https://bugzilla.mozilla.org/show_bug.cgi?id=1954934
  // When the browser updates with an existing profile, and the checkpoint
  // differs, then signature verification must be forced.
  info("Simulating browser update with forced signature change");
  Services.prefs.clearUserPref(PREF_LAST_SIGNATURE_CHECKPOINT);
  let verifiedPromise2 = TestUtils.topicObserved("xpi-signature-changed");
  await promiseStartupManager();
  Assert.notEqual(
    Services.prefs.getIntPref(PREF_LAST_TIME),
    LAST_TIMER_TRIGGERED_TIME,
    "Signature verification timer should be reset when verification is forced"
  );
  await verifiedPromise2;
  Assert.equal(
    await getSignedState(),
    AddonManager.SIGNEDSTATE_SIGNED,
    "Signature is up to date again after forced verification on startup"
  );

  await promiseShutdownManager();
});
