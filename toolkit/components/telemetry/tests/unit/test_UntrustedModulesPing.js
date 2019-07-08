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
    // This is hard-coded as untrusted in ModuleEvaluator for testing.
    {
      nameMatch: new RegExp(kDllName, "i"),
      expectedTrusted: false,
      isStartup: false,
      wasFound: false,
    },

    // These check that a DLL loaded at startup is evaluated properly.
    // This is hard-coded as untrusted in ModuleEvaluator for testing.
    {
      nameMatch: /untrusted-startup-test-dll.dll/i,
      expectedTrusted: false,
      isStartup: true,
      wasFound: false,
    },
    {
      nameMatch: /kernelbase.dll/i,
      expectedTrusted: true,
      isStartup: true,
      wasFound: false,
    },
  ];

  // There is a tiny chance some other ping is being sent legitimately before
  // the one we care about. Spin until we find the correct ping type.
  let found;
  while (true) {
    found = await PingServer.promiseNextPing();
    if (found.type == "untrustedModules") {
      break;
    }
  }

  // Test the ping payload's validity.
  Assert.ok(found, "Untrusted modules ping submitted");
  Assert.ok(found.environment, "Ping has an environment");
  Assert.ok(typeof found.clientId != "undefined", "Ping has a client ID");

  Assert.equal(found.payload.structVersion, 1, "Version is correct");
  Assert.ok(found.payload.combinedStacks, "'combinedStacks' array exists");
  Assert.ok(found.payload.events, "'events' array exists");
  Assert.equal(
    found.payload.combinedStacks.stacks.length,
    found.payload.events.length,
    "combinedStacks.length == events.length"
  );

  for (let event of found.payload.events) {
    Assert.ok(event.modules, "'modules' array exists");
    for (let mod of event.modules) {
      Assert.ok(
        typeof mod.moduleName != "undefined",
        `Module contains moduleName: ${mod.moduleName}`
      );
      Assert.ok(
        typeof mod.moduleTrustFlags != "undefined",
        `Module contains moduleTrustFlags: ${mod.moduleTrustFlags}`
      );
      Assert.ok(
        typeof mod.baseAddress != "undefined",
        "Module contains baseAddress"
      );
      Assert.ok(
        typeof mod.loaderName != "undefined",
        "Module contains loaderName"
      );
      for (let x of expectedModules) {
        if (x.nameMatch.test(mod.moduleName)) {
          x.wasFound = true;
          Assert.equal(
            x.isStartup,
            event.isStartup,
            `isStartup == expected for module: ${x.nameMatch.source}`
          );
        }
      }
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
