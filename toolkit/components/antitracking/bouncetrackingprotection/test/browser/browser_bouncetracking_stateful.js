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

// Cookie tests.

add_task(async function test_bounce_stateful_cookies_client() {
  info("Test client bounce with cookie.");
  await runTestBounce({
    bounceType: "client",
    setState: "cookie-client",
  });
  info("Test client bounce without cookie.");
  await runTestBounce({
    bounceType: "client",
    setState: null,
    expectCandidate: false,
    expectPurge: false,
  });
});

add_task(async function test_bounce_stateful_cookies_server() {
  info("Test server bounce with cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: "cookie-server",
  });
  info("Test server bounce without cookie.");
  await runTestBounce({
    bounceType: "server",
    setState: null,
    expectCandidate: false,
    expectPurge: false,
  });
});

// Storage tests.

// TODO: Bug 1848406: Implement stateful bounce detection for localStorage.
add_task(async function test_bounce_stateful_localStorage() {
  info("TODO: client bounce with localStorage.");
  await runTestBounce({
    bounceType: "client",
    setState: "localStorage",
    expectCandidate: false,
    expectPurge: false,
  });
});
