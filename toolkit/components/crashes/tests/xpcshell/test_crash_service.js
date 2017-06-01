/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/AppData.jsm", this);
Cu.import("resource://testing-common/CrashManagerTest.jsm", this);
var bsp = Cu.import("resource://gre/modules/CrashManager.jsm", {});

function run_test() {
  run_next_test();
}

add_task(async function test_instantiation() {
  Assert.ok(!bsp.gCrashManager, "CrashManager global instance not initially defined.");

  do_get_profile();
  await makeFakeAppDir();

  // Fake profile creation.
  Cc["@mozilla.org/crashservice;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", null);

  Assert.ok(bsp.gCrashManager, "Profile creation makes it available.");
  Assert.ok(Services.crashmanager, "CrashManager available via Services.");
  Assert.strictEqual(bsp.gCrashManager, Services.crashmanager,
                     "The objects are the same.");
});

add_task(async function test_addCrash() {
  const crashId = "56cd87bc-bb26-339b-3a8e-f00c0f11380e";

  let cwd = await OS.File.getCurrentDirectory();
  let minidump = OS.Path.join(cwd, "crash.dmp");
  let extra = OS.Path.join(cwd, "crash.extra");
  let dir = do_get_tempdir();

  // Make a copy of the files because the .extra file will be modified
  await OS.File.copy(minidump, OS.Path.join(dir.path, crashId + ".dmp"));
  await OS.File.copy(extra, OS.Path.join(dir.path, crashId + ".extra"));

  // Ensure that the nsICrashReporter methods can find the dump
  let crashReporter =
      Components.classes["@mozilla.org/toolkit/crash-reporter;1"]
                .getService(Components.interfaces.nsICrashReporter);
  crashReporter.minidumpPath = dir;

  let cs = Cc["@mozilla.org/crashservice;1"].getService(Ci.nsICrashService);
  await cs.addCrash(Ci.nsICrashService.PROCESS_TYPE_CONTENT,
                    Ci.nsICrashService.CRASH_TYPE_CRASH, crashId);
  let crashes = await Services.crashmanager.getCrashes();
  let crash = crashes.find(c => { return c.id === crashId; });
  Assert.ok(crash, "Crash " + crashId + " has been stored successfully.");
  Assert.equal(crash.metadata.ProcessType, "content");
  Assert.equal(crash.metadata.MinidumpSha256Hash,
    "24b0ea7794b2d2523c46c9aea72c03ccbb0ab88ad76d8258d3752c7b71d233ff");
  Assert.ok(crash.metadata.StackTraces, "The StackTraces field is present.\n")

  try {
    let stackTraces = JSON.parse(crash.metadata.StackTraces);
    Assert.equal(stackTraces.status, "OK");
    Assert.ok(stackTraces.crash_info, "The crash_info field is populated.");
    Assert.ok(stackTraces.modules && (stackTraces.modules.length > 0),
              "The module list is populated.");
    Assert.ok(stackTraces.threads && (stackTraces.modules.length > 0),
              "The thread list is populated.");

    let frames = stackTraces.threads[0].frames;
    Assert.ok(frames && (frames.length > 0), "The stack trace is present.\n");
  } catch (e) {
    Assert.ok(false, "StackTraces does not contain valid JSON.");
  }

  // Remove the minidumps to prevent the test harness from thinking we crashed
  await OS.File.removeDir(dir.path);
});
