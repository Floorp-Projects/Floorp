/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);

async function waitForConfirmationState(state, msToWait = 0) {
  await TestUtils.waitForCondition(
    () => dns.currentTrrConfirmationState == state,
    `Timed out waiting for ${state}. Currently ${dns.currentTrrConfirmationState}`,
    1,
    msToWait
  );
  equal(dns.currentTrrConfirmationState, state, "expected confirmation state");
}

const CONFIRM_OFF = 0;
const CONFIRM_TRYING_OK = 1;
const CONFIRM_OK = 2;
const CONFIRM_FAILED = 3;
const CONFIRM_TRYING_FAILED = 4;
const CONFIRM_DISABLED = 5;

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

let trrServer = null;
add_task(async function start_trr_server() {
  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port}\n`);
});

add_task(async function confirm_off() {
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  equal(dns.currentTrrConfirmationState, CONFIRM_OFF);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRROFF);
  equal(dns.currentTrrConfirmationState, CONFIRM_OFF);
});

add_task(async function confirm_disabled() {
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  equal(dns.currentTrrConfirmationState, CONFIRM_DISABLED);
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(dns.currentTrrConfirmationState, CONFIRM_DISABLED);
});

add_task(async function confirm_ok() {
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    answers: [
      {
        name: "confirm.example.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "test.com",
      },
    ],
  });
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
    `https://foo.example.com:${trrServer.port}/dns-query` // No server on this port
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await new TRRDNSListener("example.com", { expectedAnswer: "1.2.3.4" });
  await waitForConfirmationState(CONFIRM_OK, 1000);

  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    answers: [
      {
        name: "confirm.example.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "test.com",
      },
    ],
    delay: 500,
  });
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await new Promise(resolve => do_timeout(100, resolve));
  equal(
    dns.currentTrrConfirmationState,
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
  equal(dns.currentTrrConfirmationState, CONFIRM_OFF);
  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    answers: [
      {
        name: "confirm.example.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "test.com",
      },
    ],
    delay: 7000,
  });
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    dns.currentTrrConfirmationState,
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
  equal(dns.currentTrrConfirmationState, CONFIRM_OFF);
  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    error: 404,
  });
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    dns.currentTrrConfirmationState,
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
  equal(dns.currentTrrConfirmationState, CONFIRM_OFF);
  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    answers: [
      {
        name: "confirm.example.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "test.com",
      },
    ],
    delay: 100,
  });
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(
    dns.currentTrrConfirmationState,
    CONFIRM_TRYING_OK,
    "Should be CONFIRM_TRYING_OK"
  );
  await waitForConfirmationState(CONFIRM_OK, 1000);

  for (let i = 0; i < 15; i++) {
    await trrServer.registerDoHAnswers(`domain${i}.example.com`, "A", {
      error: 600,
    });
    await trrServer.registerDoHAnswers(`domain${i}.example.com`, "AAAA", {
      error: 600,
    });
  }

  let p = waitForConfirmationState(CONFIRM_TRYING_OK, 3000);
  let dnsRequests = [];
  for (let i = 0; i < 15; i++) {
    dnsRequests.push(
      new TRRDNSListener(`domain${i}.example.com`, {
        expectedAnswer: "127.0.0.1",
      })
    );
  }

  await p;
  await Promise.all(dnsRequests);
  await waitForConfirmationState(CONFIRM_OK, 1000);
});
