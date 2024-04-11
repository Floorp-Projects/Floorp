/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let bounceTrackingProtection;

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.bounceTrackingProtection.requireStatefulBounces", true],
      ["privacy.bounceTrackingProtection.bounceTrackingGracePeriodSec", 0],
    ],
  });
  bounceTrackingProtection = Cc[
    "@mozilla.org/bounce-tracking-protection;1"
  ].getService(Ci.nsIBounceTrackingProtection);
});

add_task(async function test_bounce_stateful_indexedDB() {
  info("Client bounce with indexedDB.");
  await runTestBounce({
    bounceType: "client",
    setState: "indexedDB",
    setStateInWebWorker: true,
  });
});

// FIXME: (Bug 1889898) This test is skipped because it triggers a shutdown
// hang.
add_task(async function test_bounce_stateful_indexedDB_nestedWorker() {
  info("Client bounce with indexedDB access from a nested worker.");
  await runTestBounce({
    bounceType: "client",
    setState: "indexedDB",
    setStateInNestedWebWorker: true,
  });
}).skip();
