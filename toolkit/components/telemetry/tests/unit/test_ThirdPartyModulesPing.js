/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests basic end-to-end functionality of the untrusted modules
// telemetry ping. We force the ping to fire, capture the result, and test for:
// - Basic payload structure validity.
// - Expected results for a few specific DLLs

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);
const { ctypes } = ChromeUtils.import("resource://gre/modules/ctypes.jsm");

const kDllName = "modules-test.dll";

let gDllHandle;
let gCurrentPidStr;

add_task(async function setup() {
  do_get_profile();

  // Dynamically load a DLL which we have hard-coded as untrusted; this should
  // appear in the payload.
  gDllHandle = ctypes.open(do_get_file(kDllName).path);

  // Force the timer to fire (using a small interval).
  Cc["@mozilla.org/updates/timer-manager;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "utm-test-init", "");
  Preferences.set("toolkit.telemetry.untrustedModulesPing.frequency", 0);
  Preferences.set("app.update.url", "http://localhost");

  let currentPid = Services.appinfo.processID;
  gCurrentPidStr = "0x" + currentPid.toString(16);

  // Start the local ping server and setup Telemetry to use it during the tests.
  PingServer.start();
  Preferences.set(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );

  return TelemetryController.testSetup();
});

registerCleanupFunction(function() {
  if (gDllHandle) {
    gDllHandle.close();
    gDllHandle = null;
  }
  return PingServer.stop();
});

add_task(async function test_send_ping() {
  let expectedModules = [
    // This checks that a DLL loaded during runtime is evaluated properly.
    // This is hard-coded as untrusted in toolkit/xre/UntrustedModules.cpp for
    // testing purposes.
    {
      nameMatch: new RegExp(kDllName, "i"),
      expectedTrusted: false,
      wasFound: false,
    },
    {
      nameMatch: /kernelbase.dll/i,
      expectedTrusted: true,
      wasFound: false,
    },
  ];

  // There is a tiny chance some other ping is being sent legitimately before
  // the one we care about. Spin until we find the correct ping type.
  let found;
  while (true) {
    found = await PingServer.promiseNextPing();
    if (found.type == "third-party-modules") {
      break;
    }
  }

  // Test the ping payload's validity.
  Assert.ok(found, "Untrusted modules ping submitted");
  Assert.ok(found.environment, "Ping has an environment");
  Assert.ok(typeof found.clientId != "undefined", "Ping has a client ID");

  Assert.equal(found.payload.structVersion, 1, "Version is correct");
  Assert.ok(found.payload.modules, "'modules' object exists");
  Assert.ok(Array.isArray(found.payload.modules), "'modules' is an array");
  Assert.ok(found.payload.processes, "'processes' object exists");
  Assert.ok(
    gCurrentPidStr in found.payload.processes,
    `Current process "${gCurrentPidStr}" is included in payload`
  );

  let ourProcInfo = found.payload.processes[gCurrentPidStr];
  Assert.equal(ourProcInfo.processType, "browser", "'processType' is correct");
  Assert.ok(typeof ourProcInfo.elapsed == "number", "'elapsed' exists");
  Assert.equal(
    ourProcInfo.sanitizationFailures,
    0,
    "'sanitizationFailures' is 0"
  );
  Assert.equal(ourProcInfo.trustTestFailures, 0, "'trustTestFailures' is 0");

  Assert.equal(
    ourProcInfo.combinedStacks.stacks.length,
    ourProcInfo.events.length,
    "combinedStacks.stacks.length == events.length"
  );

  for (let event of ourProcInfo.events) {
    Assert.ok(
      typeof event.processUptimeMS == "number",
      "'processUptimeMS' exists"
    );
    Assert.ok(typeof event.threadID == "number", "'threadID' exists");
    Assert.ok(typeof event.baseAddress == "string", "'baseAddress' exists");

    Assert.ok(typeof event.moduleIndex == "number", "'moduleIndex' exists");
    Assert.ok(event.moduleIndex >= 0, "'moduleIndex' is non-negative");

    Assert.ok(typeof event.isDependent == "boolean", "'isDependent' exists");
    Assert.ok(!event.isDependent, "'isDependent' is false");

    let modRecord = found.payload.modules[event.moduleIndex];
    Assert.ok(modRecord, "module record for this event exists");
    Assert.ok(
      typeof modRecord.resolvedDllName == "string",
      "'resolvedDllName' exists"
    );
    Assert.ok(typeof modRecord.trustFlags == "number", "'trustFlags' exists");

    let mod = expectedModules.find(function(elem) {
      return elem.nameMatch.test(modRecord.resolvedDllName);
    });

    if (mod) {
      mod.wasFound = true;
    }
  }

  for (let x of expectedModules) {
    Assert.equal(
      !x.wasFound,
      x.expectedTrusted,
      `Trustworthiness == expected for module: ${x.nameMatch.source}`
    );
  }
});
