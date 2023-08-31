/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { ExtensionProcessCrashObserver } = ChromeUtils.importESModule(
  "resource://gre/modules/Extension.sys.mjs"
);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "1",
  "43"
);

add_setup(() => {
  // FOG needs a profile directory to put its data in.
  do_get_profile();
  // FOG needs to be initialized in order for data to flow
  // NOTE: in mobile builds the test will pass without this call,
  // but in Desktop build the xpcshell test would get stuck on
  // the call to testGetValue.
  Services.fog.initializeFOG();
});

add_task(async function test_process_crash_telemetry() {
  await AddonTestUtils.promiseStartupManager();

  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    background() {
      browser.test.sendMessage("bg:loaded");
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg:loaded");

  let { currentProcessChildID } = ExtensionProcessCrashObserver;

  Assert.notEqual(
    currentProcessChildID,
    undefined,
    "Expect ExtensionProcessCrashObserver.currentProcessChildID to be set"
  );

  Assert.equal(
    ChromeUtils.getAllDOMProcesses().find(
      pp => pp.childID == currentProcessChildID
    )?.remoteType,
    "extension",
    "Expect a child process with remoteType extension to be found for the process childID set"
  );

  Assert.ok(
    Glean.extensions.processEvent.created.testGetValue() > 0,
    "Expect glean processEvent.created to be set"
  );

  if (ExtensionProcessCrashObserver._isAndroid) {
    Assert.ok(
      Glean.extensions.processEvent.created_fg.testGetValue() > 0,
      "Expect glean processEvent.created_fg to be set on Android builds"
    );
  }

  info("Mock application-background observer service topic");
  ExtensionProcessCrashObserver.observe(null, "application-background");

  Assert.equal(
    ExtensionProcessCrashObserver.appInForeground,
    // Only in desktop builds we expect the flag to be always true
    !ExtensionProcessCrashObserver._isAndroid,
    "Got expected value set on ExtensionProcessCrashObserver.appInForeground"
  );

  info("Mock a process crash being notified");
  let propertyBag = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
    Ci.nsIWritablePropertyBag2
  );
  propertyBag.setPropertyAsBool("abnormal", true);
  ExtensionProcessCrashObserver.observe(
    propertyBag,
    "ipc:content-shutdown",
    currentProcessChildID
  );

  Assert.ok(
    Glean.extensions.processEvent.crashed.testGetValue() > 0,
    "Expect glean processEvent.crashed to be set"
  );

  if (ExtensionProcessCrashObserver._isAndroid) {
    Assert.ok(
      Glean.extensions.processEvent.crashed_bg.testGetValue() > 0,
      "Expect glean processEvent.crashed_bg to be set on Android builds"
    );
  }

  await extension.unload();
});
