/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

"use strict";

function test() {
  waitForExplicitFinish();
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );

  // Types of processes to test, taken from GeckoProcessTypes.h
  var processTypes = ["tab", "gpu"];

  // A callback called after each test-result.
  Services.obs.addObserver(function result(subject, topic, data) {
    let { testid, shouldPermit, wasPermitted, message } = JSON.parse(data);
    ok(
      shouldPermit == wasPermitted,
      "Test " +
        testid +
        " was " +
        (wasPermitted ? "" : "not ") +
        "permitted.  | " +
        message
    );
  }, "sandbox-test-result");

  // A callback that is notified when a child process is done running tests.
  var remainingTests = processTypes.length;
  Services.obs.addObserver(_ => {
    remainingTests = remainingTests - 1;
    if (remainingTests == 0) {
      // Notify SandboxTest component that it should terminate the connection
      // with the child processes.
      comp.finishTests();
      // Notify mochitest that all process tests are complete.
      finish();
    }
  }, "sandbox-test-done");

  var comp = Cc["@mozilla.org/sandbox/sandbox-test;1"].getService(
    Ci.mozISandboxTest
  );

  comp.startTests(processTypes);
}
