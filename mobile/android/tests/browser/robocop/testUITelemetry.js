// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const EVENT_TEST1 = "_test_event_1.1";
const EVENT_TEST2 = "_test_event_2.1";
const EVENT_TEST3 = "_test_event_3.1";
const EVENT_TEST4 = "_test_event_4.1";

const METHOD_TEST1 = "_test_method_1";
const METHOD_TEST2 = "_test_method_2";

// Method.NONE is converted to an empty string after a few JSON stringifications
const METHOD_NONE = "";

const REASON_TEST1 = "_test_reason_1";
const REASON_TEST2 = "_test_reason_2";

const SESSION_STARTED_TWICE = "_test_session_started_twice.1";
const SESSION_STOPPED_TWICE = "_test_session_stopped_twice.1";

function do_check_array_eq(a1, a2) {
  do_check_eq(a1.length, a2.length);
  for (let i = 0; i < a1.length; ++i) {
    do_check_eq(a1[i], a2[i]);
  }
}

/**
 * Asserts that the given measurements are equal. Assumes that measurements
 * of type "event" have their sessions arrays sorted.
 */
function do_check_measurement_eq(m1, m2) {
  do_check_eq(m1.type, m2.type);

  switch (m1.type) {
    case "event":
      do_check_eq(m1.action, m2.action);
      do_check_eq(m1.method, m2.method);
      do_check_array_eq(m1.sessions, m2.sessions);
      do_check_eq(m1.extras, m2.extras);
      break;

    case "session":
      do_check_eq(m1.name, m2.name);
      do_check_eq(m1.reason, m2.reason);
      break;

    default:
      do_throw("Unknown event type: " + m1.type);
  }
}

function getObserver() {
  let bridge = Cc["@mozilla.org/android/bridge;1"]
                 .getService(Ci.nsIAndroidBridge);
  let obsXPCOM = bridge.browserApp.getUITelemetryObserver();
  do_check_true(!!obsXPCOM);
  return obsXPCOM.wrappedJSObject;
}

/**
 * The following event test will fail if telemetry isn't enabled. The Java-side
 * part of this test should have turned it on; fail if it didn't work.
 */
add_test(function test_enabled() {
  let obs = getObserver();
  do_check_true(!!obs);
  do_check_true(obs.enabled);
  run_next_test();
});

add_test(function test_telemetry_events() {
  let expected = expectedArraysToObjs([
    ["event",   EVENT_TEST1, METHOD_TEST1, [],                                             undefined],
    ["event",   EVENT_TEST2, METHOD_TEST1, [SESSION_STARTED_TWICE],                        undefined],
    ["event",   EVENT_TEST2, METHOD_TEST2, [SESSION_STARTED_TWICE],                        undefined],
    ["event",   EVENT_TEST3, METHOD_TEST1, [SESSION_STARTED_TWICE, SESSION_STOPPED_TWICE], "foobarextras"],
    ["session", SESSION_STARTED_TWICE, REASON_TEST1],
    ["event",   EVENT_TEST4, METHOD_TEST1, [SESSION_STOPPED_TWICE],                        "barextras"],
    ["session", SESSION_STOPPED_TWICE, REASON_TEST2],
    ["event",   EVENT_TEST1, METHOD_NONE, [],                                              undefined],
  ]);

  let obs = getObserver();
  let measurements = removeNonTestMeasurements(obs.getUIMeasurements());

  measurements.forEach(function (m, i) {
    if (m.type === "event") {
      m.sessions = removeNonTestSessions(m.sessions);
      m.sessions.sort(); // Mutates.
    }

    do_check_measurement_eq(expected[i], m);
  });

  expected.forEach(function (m, i) {
    do_check_measurement_eq(m, measurements[i]);
  });

  run_next_test();
});

/**
 * Converts the expected value arrays to objects,
 * for less typing when initializing the expected arrays.
 */
function expectedArraysToObjs(expectedArrays) {
  return expectedArrays.map(function (arr) {
    let type = arr[0];
    if (type === "event") {
      return {
        type: type,
        action: arr[1],
        method: arr[2],
        sessions: arr[3].sort(), // Sort, just in case it's not sorted by hand!
        extras: arr[4],
      };

    } else if (type === "session") {
      return {
        type: type,
        name: arr[1],
        reason: arr[2],
      };
    }
  });
}

function removeNonTestMeasurements(measurements) {
  return measurements.filter(function (measurement) {
    if (measurement.type === "event") {
      return measurement.action.startsWith("_test_event_");
    } else if (measurement.type === "session") {
      return measurement.name.startsWith("_test_session_");
    }
    return false;
  });
}

function removeNonTestSessions(sessions) {
  return sessions.filter(function (sessionName) {
    return sessionName.startsWith("_test_session_");
  });
}

run_next_test();
