/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const { getCrashManagerNoCreate } = ChromeUtils.import(
  "resource://gre/modules/CrashManager.jsm"
);
const { makeFakeAppDir } = ChromeUtils.import(
  "resource://testing-common/AppData.jsm"
);

add_task(async function test_instantiation() {
  Assert.ok(
    !getCrashManagerNoCreate(),
    "CrashManager global instance not initially defined."
  );

  do_get_profile();
  await makeFakeAppDir();

  // Fake profile creation.
  Cc["@mozilla.org/crashservice;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", null);

  Assert.ok(getCrashManagerNoCreate(), "Profile creation makes it available.");
  Assert.ok(Services.crashmanager, "CrashManager available via Services.");
  Assert.strictEqual(
    getCrashManagerNoCreate(),
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

async function addCrash(id, type = Ci.nsICrashService.CRASH_TYPE_CRASH) {
  let cs = Cc["@mozilla.org/crashservice;1"].getService(Ci.nsICrashService);
  return cs.addCrash(Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT, type, id);
}

async function getCrash(crashId) {
  let crashes = await Services.crashmanager.getCrashes();
  return crashes.find(c => {
    return c.id === crashId;
  });
}

async function test_addCrashBase(crashId, allThreads) {
  await setup(crashId);

  let crashType = Ci.nsICrashService.CRASH_TYPE_CRASH;
  if (allThreads) {
    crashType = Ci.nsICrashService.CRASH_TYPE_HANG;
  }
  await addCrash(crashId, crashType);
  let crash = await getCrash(crashId);
  Assert.ok(crash, "Crash " + crashId + " has been stored successfully.");
  Assert.equal(crash.metadata.ProcessType, "content");
  Assert.equal(
    crash.metadata.MinidumpSha256Hash,
    "c8ad56a2096310f40c8a4b46c890625a740fdd72e409f412933011ff947c5a40"
  );
  Assert.ok(crash.metadata.StackTraces, "The StackTraces field is present.\n");

  try {
    let stackTraces = crash.metadata.StackTraces;
    Assert.equal(stackTraces.status, "OK");
    Assert.ok(stackTraces.crash_info, "The crash_info field is populated.");
    Assert.ok(
      stackTraces.modules && !!stackTraces.modules.length,
      "The module list is populated."
    );
    Assert.ok(
      stackTraces.threads && !!stackTraces.threads.length,
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
    Assert.ok(frames && !!frames.length, "The stack trace is present.\n");
  } catch (e) {
    Assert.ok(false, "StackTraces does not contain valid JSON.");
  }

  try {
    let telemetryEnvironment = JSON.parse(crash.metadata.TelemetryEnvironment);
    Assert.equal(telemetryEnvironment.EscapedField, "EscapedData\n\nfoo");
  } catch (e) {
    Assert.ok(
      false,
      "TelemetryEnvironment contents were not properly escaped\n"
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

add_task(async function test_addCrash_shutdownOnCrash() {
  const crashId = "de7f63dd-7516-4525-a44b-6d2f2bd3934a";
  await setup(crashId);

  // Set the MOZ_CRASHREPORTER_SHUTDOWN environment variable
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  env.set("MOZ_CRASHREPORTER_SHUTDOWN", "1");

  await addCrash(crashId);

  let crash = await getCrash(crashId);
  Assert.ok(crash, "Crash " + crashId + " has been stored successfully.");
  Assert.ok(
    crash.metadata.StackTraces === undefined,
    "The StackTraces field is not present because the minidump " +
      "analyzer did not start.\n"
  );

  env.set("MOZ_CRASHREPORTER_SHUTDOWN", ""); // Unset the environment variable
  await teardown();
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

  let addCrashPromise = addCrash(firstCrashId);

  // Spin the event loop so that the minidump analyzer is launched
  await new Promise(resolve => {
    executeSoon(resolve);
  });

  // Pretend we're quitting
  let cs = Cc["@mozilla.org/crashservice;1"].getService(Ci.nsICrashService);
  let obs = cs.QueryInterface(Ci.nsIObserver);
  obs.observe(null, "quit-application", null);

  // Wait for the minidump analyzer to be killed
  await minidumpAnalyzerKilledPromise;

  // Now wait for the crash to be recorded
  await addCrashPromise;
  let crash = await getCrash(firstCrashId);
  Assert.ok(crash, "Crash " + firstCrashId + " has been stored successfully.");

  // Cleanup the fake crash and generate a new one
  await teardown();
  await setup(secondCrashId);

  await addCrash(secondCrashId);
  crash = await getCrash(secondCrashId);
  Assert.ok(crash, "Crash " + secondCrashId + " has been stored successfully.");
  Assert.ok(
    crash.metadata.StackTraces === undefined,
    "The StackTraces field is not present because the minidump " +
      "analyzer did not start.\n"
  );
  await teardown();
});
