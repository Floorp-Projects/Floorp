/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://testing-common/TelemetryArchiveTesting.jsm", this);

ChromeUtils.defineModuleGetter(this, "TelemetryEventPing",
                               "resource://gre/modules/TelemetryEventPing.jsm");

function checkPingStructure(type, payload, options) {
  Assert.equal(type, TelemetryEventPing.EVENT_PING_TYPE, "Should be an event ping.");
  // Check the payload for required fields.
  Assert.ok("reason" in payload, "Payload must have reason.");
  Assert.ok("processStartTimestamp" in payload, "Payload must have processStartTimestamp.");
  Assert.ok("sessionId" in payload, "Payload must have sessionId.");
  Assert.ok("subsessionId" in payload, "Payload must have subsessionId.");
  Assert.ok("lostEventsCount" in payload, "Payload must have lostEventsCount.");
  Assert.ok("events" in payload, "Payload must have events.");
}

function fakePolicy(set, clear, send) {
  let mod = ChromeUtils.import("resource://gre/modules/TelemetryEventPing.jsm", {});
  mod.Policy.setTimeout = set;
  mod.Policy.clearTimeout = clear;
  mod.Policy.sendPing = send;
}

function pass() { /* intentionally empty */ }
function fail() {
  Assert.ok(false, "Not allowed");
}

function recordEvents(howMany) {
  for (let i = 0; i < howMany; i++) {
    Telemetry.recordEvent("telemetry.test", "test1", "object1");
  }
}

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  await TelemetryController.testSetup();
  TelemetryEventPing.testReset();
  Telemetry.setEventRecordingEnabled("telemetry.test", true);
});

// Tests often take the form of faking policy within faked policy.
// This is to allow us to record events in addition to any that were
// recorded to trigger the submit in the first place.
// This works because we start the timer at the top of _submitPing, giving us
// this opportunity.
// This results in things looking this way:
/*
fakePolicy((callback, delay) => {
  // Code that runs at the top of _submitPing
  fakePolicy(pass, pass, (type, payload, options) => {
    // Code that runs at the bottom of _submitPing
  });
}, pass, fail);
// Code that triggers _submitPing to run
*/

add_task(async function test_eventLimitReached() {
  Telemetry.clearEvents();
  TelemetryEventPing.testReset();

  let pingCount = 0;

  fakePolicy(pass, pass, fail);
  recordEvents(999);
  fakePolicy((callback, delay) => {
    Telemetry.recordEvent("telemetry.test", "test2", "object1");
    fakePolicy(pass, pass, (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.ok(options.addClientId, "Adds the client id.");
      Assert.ok(options.addEnvironment, "Adds the environment.");
      Assert.ok(!options.usePingSender, "Doesn't require pingsender.");
      Assert.equal(payload.reason, TelemetryEventPing.Reason.MAX, "Sending because we hit max");
      Assert.equal(payload.events.parent.length, 1000, "Has one thousand events");
      Assert.equal(payload.lostEventsCount, 0, "Lost no events");
      Assert.ok(!payload.events.parent.some(ev => ev[1] === "test2"), "Should not have included the final event (yet).");
      pingCount++;
    });
  }, pass, fail);
  // Now trigger the submit.
  Telemetry.recordEvent("telemetry.test", "test1", "object1");
  Assert.equal(pingCount, 1, "Should have sent a ping");

  // With a recent MAX ping sent, record another max amount of events (and then two extras).
  fakePolicy(fail, fail, fail);
  recordEvents(998);
  fakePolicy((callback, delay) => {
    Telemetry.recordEvent("telemetry.test", "test2", "object2");
    Telemetry.recordEvent("telemetry.test", "test2", "object2");
    fakePolicy(pass, pass, (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.ok(options.addClientId, "Adds the client id.");
      Assert.ok(options.addEnvironment, "Adds the environment.");
      Assert.ok(!options.usePingSender, "Doesn't require pingsender.");
      Assert.equal(payload.reason, TelemetryEventPing.Reason.MAX, "Sending because we hit max");
      Assert.equal(payload.events.parent.length, 1000, "Has one thousand events");
      Assert.equal(payload.lostEventsCount, 2, "Lost two events");
      Assert.equal(payload.events.parent[0][2], "test2", "The first event of the second bunch should be the leftover event of the first bunch.");
      Assert.ok(!payload.events.parent.some(ev => ev[3] === "object2"), "Should not have included any of the lost two events.");
      pingCount++;
    });
    callback(); // Trigger the send immediately.
  }, pass, fail);
  recordEvents(1);
  Assert.equal(pingCount, 2, "Should have sent a second ping");

  // Ensure we send a subsequent MAX ping exactly on 1000 events, and without
  // the two events we lost.
  fakePolicy(fail, fail, fail);
  recordEvents(999);
  fakePolicy((callback, delay) => {
    fakePolicy(pass, pass, (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.ok(options.addClientId, "Adds the client id.");
      Assert.ok(options.addEnvironment, "Adds the environment.");
      Assert.ok(!options.usePingSender, "Doesn't require pingsender.");
      Assert.equal(payload.reason, TelemetryEventPing.Reason.MAX, "Sending because we hit max");
      Assert.equal(payload.events.parent.length, 1000, "Has one thousand events");
      Assert.equal(payload.lostEventsCount, 0, "Lost no events");
      Assert.ok(!payload.events.parent.some(ev => ev[3] === "object2"), "Should not have included any of the lost two events from the previous bunch.");
      pingCount++;
    });
    callback(); // Trigger the send immediately
  });
  recordEvents(1);
  Assert.equal(pingCount, 3, "Should have sent a third ping");

});

add_task(async function test_timers() {
  Telemetry.clearEvents();
  TelemetryEventPing.testReset();

  // Immediately after submitting a MAX ping, we should set the timer for the
  // next interval.
  recordEvents(999);
  fakePolicy((callback, delay) => {
    Assert.equal(delay, TelemetryEventPing.minFrequency, "Timer should be started with the min frequency");
  }, pass, pass);
  recordEvents(1);

  fakePolicy((callback, delay) => {
    Assert.ok(delay <= TelemetryEventPing.maxFrequency, "Timer should be at most the max frequency for a subsequent MAX ping.");
  }, pass, pass);
  recordEvents(1000);

});

add_task(async function test_shutdown() {
  Telemetry.clearEvents();
  TelemetryEventPing.testReset();

  recordEvents(999);
  fakePolicy(pass, pass, (type, payload, options) => {
    Assert.ok(options.addClientId, "Adds the client id.");
    Assert.ok(options.addEnvironment, "Adds the environment.");
    Assert.ok(options.usePingSender, "Asks for pingsender.");
    Assert.equal(payload.reason, TelemetryEventPing.Reason.SHUTDOWN, "Sending because we are shutting down");
    Assert.equal(payload.events.parent.length, 999, "Has 999 events");
    Assert.equal(payload.lostEventsCount, 0, "No lost events");
  });
  TelemetryEventPing.observe(null, "profile-before-change", null);
});

add_task(async function test_periodic() {
  Telemetry.clearEvents();
  TelemetryEventPing.testReset();

  fakePolicy((callback, delay) => {
    Assert.equal(delay, TelemetryEventPing.minFrequency, "Timer should default to the min frequency");
    fakePolicy(pass, pass, (type, payload, options) => {
      checkPingStructure(type, payload, options);
      Assert.ok(options.addClientId, "Adds the client id.");
      Assert.ok(options.addEnvironment, "Adds the environment.");
      Assert.ok(!options.usePingSender, "Doesn't require pingsender.");
      Assert.equal(payload.reason, TelemetryEventPing.Reason.PERIODIC, "Sending because we hit a timer");
      Assert.equal(payload.events.parent.length, 1, "Has one event");
      Assert.equal(payload.lostEventsCount, 0, "Lost no events");
    });
    callback();
  }, pass, fail);

  recordEvents(1);
  TelemetryEventPing._startTimer();
});
