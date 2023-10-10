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
  Services.fog.testResetFOG();

  Assert.equal(
    ExtensionProcessCrashObserver.appInForeground,
    AppConstants.platform !== "android",
    "Expect appInForeground to be initially true on desktop and false on android builds"
  );

  // For Android build we mock the app moving in the foreground for the first time
  // (which, in a real Fenix instance, happens when the application receives the first
  // call to the onPause lifecycle callback and the geckoview-initial-foreground
  // topic is being notified to Gecko as a side-effect of that).
  //
  // We have to mock the app moving in the foreground before any of the test extension
  // startup, so that both Desktop and Mobile builds are in the same initial foreground
  // state for the rest of the test file.
  if (AppConstants.platform === "android") {
    info("Mock geckoview-initial-foreground observer service topic");
    ExtensionProcessCrashObserver.observe(null, "geckoview-initial-foreground");
    Assert.equal(
      ExtensionProcessCrashObserver.appInForeground,
      true,
      "Expect appInForeground to be true after geckoview-initial-foreground topic"
    );
  }
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
    Glean.extensions.processEvent.created_fg.testGetValue() > 0,
    "Expect glean processEvent.created_fg to be set."
  );
  Assert.equal(
    undefined,
    Glean.extensions.processEvent.created_bg.testGetValue(),
    "Expect glean processEvent.created_bg to be not set."
  );

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

  if (ExtensionProcessCrashObserver._isAndroid) {
    Assert.ok(
      Glean.extensions.processEvent.crashed_bg.testGetValue() > 0,
      "Expect glean processEvent.crashed_bg to be set on Android builds."
    );
  } else {
    Assert.ok(
      Glean.extensions.processEvent.crashed_fg.testGetValue() > 0,
      "Expect glean processEvent.crashed_fg to be set on desktop."
    );
  }

  await extension.unload();
});
