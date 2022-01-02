/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { TelemetryArchive } = ChromeUtils.import(
  "resource://gre/modules/TelemetryArchive.jsm"
);
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { TelemetryEnvironment } = ChromeUtils.import(
  "resource://gre/modules/TelemetryEnvironment.jsm"
);

const MS_IN_ONE_HOUR = 60 * 60 * 1000;
const MS_IN_ONE_DAY = 24 * MS_IN_ONE_HOUR;

const PREF_BRANCH = "toolkit.telemetry.";

const REASON_ABORTED_SESSION = "aborted-session";
const REASON_DAILY = "daily";
const REASON_ENVIRONMENT_CHANGE = "environment-change";
const REASON_SHUTDOWN = "shutdown";

XPCOMUtils.defineLazyGetter(this, "DATAREPORTING_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, "datareporting");
});

var promiseValidateArchivedPings = async function(aExpectedReasons) {
  // The list of ping reasons which mark the session end (and must reset the subsession
  // count).
  const SESSION_END_PING_REASONS = new Set([
    REASON_ABORTED_SESSION,
    REASON_SHUTDOWN,
  ]);

  let list = await TelemetryArchive.promiseArchivedPingList();

  // We're just interested in the "main" pings.
  list = list.filter(p => p.type == "main");

  Assert.equal(
    aExpectedReasons.length,
    list.length,
    "All the expected pings must be received."
  );

  let previousPing = await TelemetryArchive.promiseArchivedPingById(list[0].id);
  Assert.equal(
    aExpectedReasons.shift(),
    previousPing.payload.info.reason,
    "Telemetry should only get pings with expected reasons."
  );
  Assert.equal(
    previousPing.payload.info.previousSessionId,
    null,
    "The first session must report a null previous session id."
  );
  Assert.equal(
    previousPing.payload.info.previousSubsessionId,
    null,
    "The first subsession must report a null previous subsession id."
  );
  Assert.equal(
    previousPing.payload.info.profileSubsessionCounter,
    1,
    "profileSubsessionCounter must be 1 the first time."
  );
  Assert.equal(
    previousPing.payload.info.subsessionCounter,
    1,
    "subsessionCounter must be 1 the first time."
  );

  let expectedSubsessionCounter = 1;
  let expectedPreviousSessionId = previousPing.payload.info.sessionId;

  for (let i = 1; i < list.length; i++) {
    let currentPing = await TelemetryArchive.promiseArchivedPingById(
      list[i].id
    );
    let currentInfo = currentPing.payload.info;
    let previousInfo = previousPing.payload.info;
    info(
      "Archive entry " +
        i +
        " - id: " +
        currentPing.id +
        ", reason: " +
        currentInfo.reason
    );

    Assert.equal(
      aExpectedReasons.shift(),
      currentInfo.reason,
      "Telemetry should only get pings with expected reasons."
    );
    Assert.equal(
      currentInfo.previousSessionId,
      expectedPreviousSessionId,
      "Telemetry must correctly chain session identifiers."
    );
    Assert.equal(
      currentInfo.previousSubsessionId,
      previousInfo.subsessionId,
      "Telemetry must correctly chain subsession identifiers."
    );
    Assert.equal(
      currentInfo.profileSubsessionCounter,
      previousInfo.profileSubsessionCounter + 1,
      "Telemetry must correctly track the profile subsessions count."
    );
    Assert.equal(
      currentInfo.subsessionCounter,
      expectedSubsessionCounter,
      "The subsession counter should be monotonically increasing."
    );

    // Store the current ping as previous.
    previousPing = currentPing;
    // Reset the expected subsession counter, if required. Otherwise increment the expected
    // subsession counter.
    // If this is the final subsession of a session we need to update expected values accordingly.
    if (SESSION_END_PING_REASONS.has(currentInfo.reason)) {
      expectedSubsessionCounter = 1;
      expectedPreviousSessionId = currentInfo.sessionId;
    } else {
      expectedSubsessionCounter++;
    }
  }
};

add_task(async function test_setup() {
  do_test_pending();

  // Addon manager needs a profile directory
  do_get_profile();
  await loadAddonManager(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1.9.2"
  );
  finishAddonManagerStartup();
  fakeIntlReady();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();
});

add_task(async function test_subsessionsChaining() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android, so skip the next checks.
    return;
  }

  const PREF_TEST = PREF_BRANCH + "test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, { what: TelemetryEnvironment.RECORD_PREF_VALUE }],
  ]);
  Preferences.reset(PREF_TEST);

  // Fake the clock data to manually trigger an aborted-session ping and a daily ping.
  // This is also helpful to make sure we get the archived pings in an expected order.
  let now = fakeNow(2009, 9, 18, 0, 0, 0);
  let monotonicNow = fakeMonotonicNow(1000);

  let moveClockForward = minutes => {
    let ms = minutes * MILLISECONDS_PER_MINUTE;
    now = fakeNow(futureDate(now, ms));
    monotonicNow = fakeMonotonicNow(monotonicNow + ms);
  };

  // Keep track of the ping reasons we're expecting in this test.
  let expectedReasons = [];

  // Start and shut down Telemetry. We expect a shutdown ping with profileSubsessionCounter: 1,
  // subsessionCounter: 1, subsessionId: A,  and previousSubsessionId: null to be archived.
  await TelemetryController.testSetup();
  await TelemetryController.testShutdown();
  expectedReasons.push(REASON_SHUTDOWN);

  // Start Telemetry but don't wait for it to initialise before shutting down. We expect a
  // shutdown ping with profileSubsessionCounter: 2, subsessionCounter: 1, subsessionId: B
  // and previousSubsessionId: A to be archived.
  moveClockForward(30);
  TelemetryController.testReset();
  await TelemetryController.testShutdown();
  expectedReasons.push(REASON_SHUTDOWN);

  // Start Telemetry and simulate an aborted-session ping. We expect an aborted-session ping
  // with profileSubsessionCounter: 3, subsessionCounter: 1, subsessionId: C and
  // previousSubsessionId: B to be archived.
  let schedulerTickCallback = null;
  fakeSchedulerTimer(
    callback => (schedulerTickCallback = callback),
    () => {}
  );
  await TelemetryController.testReset();
  moveClockForward(6);
  // Trigger the an aborted session ping save. When testing,we are not saving the aborted-session
  // ping as soon as Telemetry starts, otherwise we would end up with unexpected pings being
  // sent when calling |TelemetryController.testReset()|, thus breaking some tests.
  Assert.ok(!!schedulerTickCallback);
  await schedulerTickCallback();
  expectedReasons.push(REASON_ABORTED_SESSION);

  // Start Telemetry and trigger an environment change through a pref modification. We expect
  // an environment-change ping with profileSubsessionCounter: 4, subsessionCounter: 1,
  // subsessionId: D and previousSubsessionId: C to be archived.
  moveClockForward(30);
  await TelemetryController.testReset();
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  moveClockForward(30);
  Preferences.set(PREF_TEST, 1);
  expectedReasons.push(REASON_ENVIRONMENT_CHANGE);

  // Shut down Telemetry. We expect a shutdown ping with profileSubsessionCounter: 5,
  // subsessionCounter: 2, subsessionId: E and previousSubsessionId: D to be archived.
  moveClockForward(30);
  await TelemetryController.testShutdown();
  expectedReasons.push(REASON_SHUTDOWN);

  // Start Telemetry and trigger a daily ping. We expect a daily ping with
  // profileSubsessionCounter: 6, subsessionCounter: 1, subsessionId: F and
  // previousSubsessionId: E to be archived.
  moveClockForward(30);
  await TelemetryController.testReset();

  // Delay the callback around midnight.
  now = fakeNow(futureDate(now, MS_IN_ONE_DAY));
  // Trigger the daily ping.
  await schedulerTickCallback();
  expectedReasons.push(REASON_DAILY);

  // Trigger an environment change ping. We expect an environment-changed ping with
  // profileSubsessionCounter: 7, subsessionCounter: 2, subsessionId: G and
  // previousSubsessionId: F to be archived.
  moveClockForward(30);
  Preferences.set(PREF_TEST, 0);
  expectedReasons.push(REASON_ENVIRONMENT_CHANGE);

  // Shut down Telemetry and trigger a shutdown ping.
  moveClockForward(30);
  await TelemetryController.testShutdown();
  expectedReasons.push(REASON_SHUTDOWN);

  // Start Telemetry and trigger an environment change.
  await TelemetryController.testReset();
  await TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  moveClockForward(30);
  Preferences.set(PREF_TEST, 1);
  expectedReasons.push(REASON_ENVIRONMENT_CHANGE);

  // Don't shut down, instead trigger an aborted-session ping.
  moveClockForward(6);
  // Trigger the an aborted session ping save.
  await schedulerTickCallback();
  expectedReasons.push(REASON_ABORTED_SESSION);

  // Start Telemetry and trigger a daily ping.
  moveClockForward(30);
  await TelemetryController.testReset();
  // Delay the callback around midnight.
  now = futureDate(now, MS_IN_ONE_DAY);
  fakeNow(now);
  // Trigger the daily ping.
  await schedulerTickCallback();
  expectedReasons.push(REASON_DAILY);

  // Trigger an environment change.
  moveClockForward(30);
  Preferences.set(PREF_TEST, 0);
  expectedReasons.push(REASON_ENVIRONMENT_CHANGE);

  // And an aborted-session ping again.
  moveClockForward(6);
  // Trigger the an aborted session ping save.
  await schedulerTickCallback();
  expectedReasons.push(REASON_ABORTED_SESSION);

  // Make sure the aborted-session ping gets archived.
  await TelemetryController.testReset();

  await promiseValidateArchivedPings(expectedReasons);
});

add_task(async function() {
  await TelemetryController.testShutdown();
  do_test_finished();
});
