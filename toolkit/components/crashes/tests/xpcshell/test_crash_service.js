/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/osfile.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/AppData.jsm", this);
ChromeUtils.import("resource://testing-common/CrashManagerTest.jsm", this);
var bsp = ChromeUtils.import("resource://gre/modules/CrashManager.jsm", null);

add_task(async function test_instantiation() {
  Assert.ok(
    !bsp.gCrashManager,
    "CrashManager global instance not initially defined."
  );

  do_get_profile();
  await makeFakeAppDir();

  // Fake profile creation.
  Cc["@mozilla.org/crashservice;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", null);

  Assert.ok(bsp.gCrashManager, "Profile creation makes it available.");
  Assert.ok(Services.crashmanager, "CrashManager available via Services.");
  Assert.strictEqual(
    bsp.gCrashManager,
    Services.crashmanager,
    "The objects are the same."
  );
});

var gMinidumpDir = do_get_tempdir();
var gCrashReporter = Cc["@mozilla.org/toolkit/crash-reporter;1"].getService(
  Ci.nsICrashReporter
);

// Ensure that the nsICrashReporter methods can find the dump
gCrashReporter.minidumpPath = gMinidumpDir;

var gDumpFile;
var gExtraFile;

// Sets up a fake crash dump and sets up the crashreporter so that it will be
// able to find it.
async function setup(crashId) {
  let cwd = await OS.File.getCurrentDirectory();
  let minidump = OS.Path.join(cwd, "crash.dmp");
  let extra = OS.Path.join(cwd, "crash.extra");

  // Make a copy of the files because the .extra file will be modified
  gDumpFile = OS.Path.join(gMinidumpDir.path, crashId + ".dmp");
  await OS.File.copy(minidump, gDumpFile);
  gExtraFile = OS.Path.join(gMinidumpDir.path, crashId + ".extra");
  await OS.File.copy(extra, gExtraFile);
}

// Cleans up the fake crash dump and resets the minidump path
async function teardown() {
  await OS.File.remove(gDumpFile);
  await OS.File.remove(gExtraFile);
}

async function test_addCrashBase(crashId, allThreads) {
  await setup(crashId);

  let crashType = Ci.nsICrashService.CRASH_TYPE_CRASH;
  if (allThreads) {
    crashType = Ci.nsICrashService.CRASH_TYPE_HANG;
  }
  let cs = Cc["@mozilla.org/crashservice;1"].getService(Ci.nsICrashService);
  await cs.addCrash(
    Ci.nsICrashService.PROCESS_TYPE_CONTENT,
    crashType,
    crashId
  );
  let crashes = await Services.crashmanager.getCrashes();
  let crash = crashes.find(c => {
    return c.id === crashId;
  });
  Assert.ok(crash, "Crash " + crashId + " has been stored successfully.");
  Assert.equal(crash.metadata.ProcessType, "content");
  Assert.equal(
    crash.metadata.MinidumpSha256Hash,
    "c8ad56a2096310f40c8a4b46c890625a740fdd72e409f412933011ff947c5a40"
  );
  Assert.ok(crash.metadata.StackTraces, "The StackTraces field is present.\n");

  try {
    let stackTraces = JSON.parse(crash.metadata.StackTraces);
    Assert.equal(stackTraces.status, "OK");
    Assert.ok(stackTraces.crash_info, "The crash_info field is populated.");
    Assert.ok(
      stackTraces.modules && stackTraces.modules.length > 0,
      "The module list is populated."
    );
    Assert.ok(
      stackTraces.threads && stackTraces.threads.length > 0,
      "The thread list is populated."
    );

    if (allThreads) {
      Assert.ok(
        stackTraces.threads.length > 1,
        "The stack trace contains more than one thread."
      );
    } else {
      Assert.ok(
        stackTraces.threads.length == 1,
        "The stack trace contains exactly one thread."
      );
    }

    let frames = stackTraces.threads[0].frames;
    Assert.greater(frames && frames.length, 0, "The stack trace is present.\n");
  } catch (e) {
    Assert.ok(false, "StackTraces does not contain valid JSON.");
  }

  try {
    let telemetryEnvironment = JSON.parse(crash.metadata.TelemetryEnvironment);
    Assert.equal(telemetryEnvironment.EscapedField, "EscapedData\n\nfoo");
  } catch (e) {
    Assert.ok(
      false,
      "TelemetryEnvironment contents were not properly re-escaped\n"
    );
  }

  await teardown();
}

add_task(async function test_addCrash() {
  await test_addCrashBase("56cd87bc-bb26-339b-3a8e-f00c0f11380e", false);
});

add_task(async function test_addCrashAllThreads() {
  await test_addCrashBase("071843c4-da89-4447-af9f-965163e0b253", true);
});

add_task(async function test_addCrash_quitting() {
  const firstCrashId = "0e578a74-a887-48cb-b270-d4775d01e715";
  const secondCrashId = "208379e5-1979-430d-a066-f6e57a8130ce";

  await setup(firstCrashId);

  let minidumpAnalyzerKilledPromise = new Promise((resolve, reject) => {
    Services.obs.addObserver((subject, topic, data) => {
      if (topic === "test-minidump-analyzer-killed") {
        resolve();
      }

      reject();
    }, "test-minidump-analyzer-killed");
  });

  let cs = Cc["@mozilla.org/crashservice;1"].getService(Ci.nsICrashService);
  let addCrashPromise = cs.addCrash(
    Ci.nsICrashService.PROCESS_TYPE_CONTENT,
    Ci.nsICrashService.CRASH_TYPE_CRASH,
    firstCrashId
  );

  // Spin the event loop so that the minidump analyzer is launched
  await new Promise(resolve => {
    executeSoon(resolve);
  });

  // Pretend we're quitting
  let obs = cs.QueryInterface(Ci.nsIObserver);
  obs.observe(null, "quit-application", null);

  // Wait for the minidump analyzer to be killed
  await minidumpAnalyzerKilledPromise;

  // Now wait for the crash to be recorded
  await addCrashPromise;
  let crashes = await Services.crashmanager.getCrashes();
  let crash = crashes.find(c => {
    return c.id === firstCrashId;
  });
  Assert.ok(crash, "Crash " + firstCrashId + " has been stored successfully.");

  // Cleanup the fake crash and generate a new one
  await teardown();
  await setup(secondCrashId);

  await cs.addCrash(
    Ci.nsICrashService.PROCESS_TYPE_CONTENT,
    Ci.nsICrashService.CRASH_TYPE_CRASH,
    secondCrashId
  );
  crashes = await Services.crashmanager.getCrashes();
  crash = crashes.find(c => {
    return c.id === secondCrashId;
  });
  Assert.ok(crash, "Crash " + secondCrashId + " has been stored successfully.");
  Assert.ok(
    crash.metadata.StackTraces === undefined,
    "The StackTraces field is not present because the minidump " +
      "analyzer did not start.\n"
  );
  await teardown();
});
