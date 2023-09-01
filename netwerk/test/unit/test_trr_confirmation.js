/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

async function waitForConfirmationState(state, msToWait = 0) {
  await TestUtils.waitForCondition(
    () => Services.dns.currentTrrConfirmationState == state,
    `Timed out waiting for ${state}. Currently ${Services.dns.currentTrrConfirmationState}`,
    1,
    msToWait
  );
  equal(
    Services.dns.currentTrrConfirmationState,
    state,
    "expected confirmation state"
  );
}

const CONFIRM_OFF = 0;
const CONFIRM_TRYING_OK = 1;
const CONFIRM_OK = 2;
const CONFIRM_FAILED = 3;
const CONFIRM_TRYING_FAILED = 4;
const CONFIRM_DISABLED = 5;

function setup() {
  trr_test_setup();
  Services.prefs.setBoolPref("network.trr.skip-check-for-blocked-host", true);
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.trr.skip-check-for-blocked-host");
});

let trrServer = null;
add_task(async function start_trr_server() {
  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port()}\n`);

  await trrServer.registerDoHAnswers(`faily.com`, "NS", {
    answers: [
      {
        name: "faily.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "ns.faily.com",
      },
    ],
  });

  for (let i = 0; i < 15; i++) {
    await trrServer.registerDoHAnswers(`failing-domain${i}.faily.com`, "A", {
      error: 600,
    });
    await trrServer.registerDoHAnswers(`failing-domain${i}.faily.com`, "AAAA", {
      error: 600,
    });
  }
});

function trigger15Failures() {
  // We need to clear the cache in case a previous call to this method
  // put the results in the DNS cache.
  Services.dns.clearCache(true);

  let dnsRequests = [];
  // There are actually two TRR requests sent for A and AAAA records, so doing
  // DNS query 10 times should be enough to trigger confirmation process.
  for (let i = 0; i < 10; i++) {
    dnsRequests.push(
      new TRRDNSListener(`failing-domain${i}.faily.com`, {
        expectedAnswer: "127.0.0.1",
      })
    );
  }

  return Promise.all(dnsRequests);
}

async function registerNS(delay) {
  return trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    answers: [
      {
        name: "confirm.example.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "test.com",
      },
    ],
    delay,
  });
}

add_task(async function confirm_off() {
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OFF);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRROFF);
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OFF);
});

add_task(async function confirm_disabled() {
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_DISABLED);
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_DISABLED);
});

add_task(async function confirm_ok() {
  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  await registerNS(0);
  await trrServer.registerDoHAnswers("example.com", "A", {
    answers: [
      {
        name: "example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await new TRRDNSListener("example.com", { expectedAnswer: "1.2.3.4" });
  equal(await trrServer.requestCount("example.com", "A"), 1);
  await waitForConfirmationState(CONFIRM_OK, 1000);

  await registerNS(500);
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await new Promise(resolve => do_timeout(100, resolve));
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Confirmation should still be pending"
  );
  await waitForConfirmationState(CONFIRM_OK, 1000);
});

add_task(async function confirm_timeout() {
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OFF);
  await registerNS(7000);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_FAILED, 7500);
  // After the confirmation fails, a timer will periodically trigger a retry
  // causing the state to go into CONFIRM_TRYING_FAILED.
  await waitForConfirmationState(CONFIRM_TRYING_FAILED, 500);
});

add_task(async function confirm_fail_fast() {
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OFF);
  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    error: 404,
  });
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_FAILED, 100);
});

add_task(async function multiple_failures() {
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OFF);

  await registerNS(100);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_OK, 1000);
  await registerNS(4000);
  let failures = trigger15Failures();
  await waitForConfirmationState(CONFIRM_TRYING_OK, 3000);
  await failures;
  // Check that failures during confirmation are ignored.
  await trigger15Failures();
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_OK, 4500);
});

add_task(async function test_connectivity_change() {
  await registerNS(100);
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  let confirmationCount = await trrServer.requestCount(
    "confirm.example.com",
    "NS"
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_OK, 1000);
  equal(
    await trrServer.requestCount("confirm.example.com", "NS"),
    confirmationCount + 1
  );
  Services.obs.notifyObservers(
    null,
    "network:captive-portal-connectivity",
    "clear"
  );
  // This means a CP check completed successfully. But no CP was previously
  // detected, so this is mostly a no-op.
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OK);

  Services.obs.notifyObservers(
    null,
    "network:captive-portal-connectivity",
    "captive"
  );
  // This basically a successful CP login event. Wasn't captive before.
  // Still treating as a no-op.
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OK);

  // This makes the TRR service set mCaptiveIsPassed=false
  Services.obs.notifyObservers(
    null,
    "captive-portal-login",
    "{type: 'captive-portal-login', id: 0, url: 'http://localhost/'}"
  );

  await registerNS(500);
  let failures = trigger15Failures();
  // The failure should cause us to go into CONFIRM_TRYING_OK and do an NS req
  await waitForConfirmationState(CONFIRM_TRYING_OK, 3000);
  await failures;

  // The notification sets mCaptiveIsPassed=true then triggers an entirely new
  // confirmation.
  Services.obs.notifyObservers(
    null,
    "network:captive-portal-connectivity",
    "clear"
  );
  // The notification should cause us to send a new confirmation request
  equal(
    Services.dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_OK, 1000);
  // two extra confirmation events should have been received by the server
  equal(
    await trrServer.requestCount("confirm.example.com", "NS"),
    confirmationCount + 3
  );
});

add_task(async function test_network_change() {
  let confirmationCount = await trrServer.requestCount(
    "confirm.example.com",
    "NS"
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OK);

  Services.obs.notifyObservers(null, "network:link-status-changed", "up");
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OK);
  equal(
    await trrServer.requestCount("confirm.example.com", "NS"),
    confirmationCount
  );

  let failures = trigger15Failures();
  // The failure should cause us to go into CONFIRM_TRYING_OK and do an NS req
  await waitForConfirmationState(CONFIRM_TRYING_OK, 3000);
  await failures;
  // The network up event should reset the confirmation to TRYING_OK and do
  // another NS req
  Services.obs.notifyObservers(null, "network:link-status-changed", "up");
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_TRYING_OK);
  await waitForConfirmationState(CONFIRM_OK, 1000);
  // two extra confirmation events should have been received by the server
  equal(
    await trrServer.requestCount("confirm.example.com", "NS"),
    confirmationCount + 2
  );
});

add_task(async function test_uri_pref_change() {
  let confirmationCount = await trrServer.requestCount(
    "confirm.example.com",
    "NS"
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_OK);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query?changed`
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_TRYING_OK);
  await waitForConfirmationState(CONFIRM_OK, 1000);
  equal(
    await trrServer.requestCount("confirm.example.com", "NS"),
    confirmationCount + 1
  );
});

add_task(async function test_autodetected_uri() {
  const defaultPrefBranch = Services.prefs.getDefaultBranch("");
  let defaultURI = defaultPrefBranch.getCharPref(
    "network.trr.default_provider_uri"
  );
  defaultPrefBranch.setCharPref(
    "network.trr.default_provider_uri",
    `https://foo.example.com:${trrServer.port()}/dns-query?changed`
  );
  // For setDetectedTrrURI to work we must pretend we are using the default.
  Services.prefs.clearUserPref("network.trr.uri");
  await waitForConfirmationState(CONFIRM_OK, 1000);
  let confirmationCount = await trrServer.requestCount(
    "confirm.example.com",
    "NS"
  );
  Services.dns.setDetectedTrrURI(
    `https://foo.example.com:${trrServer.port()}/dns-query?changed2`
  );
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_TRYING_OK);
  await waitForConfirmationState(CONFIRM_OK, 1000);
  equal(
    await trrServer.requestCount("confirm.example.com", "NS"),
    confirmationCount + 1
  );

  // reset the default URI
  defaultPrefBranch.setCharPref("network.trr.default_provider_uri", defaultURI);
});
