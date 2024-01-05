/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/* General Partial MAR File Patch Apply Test */

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

const { BackgroundTasksTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BackgroundTasksTestUtils.sys.mjs"
);
BackgroundTasksTestUtils.init(this);
const do_backgroundtask = BackgroundTasksTestUtils.do_backgroundtask.bind(
  BackgroundTasksTestUtils
);

async function run_test() {
  // Without omnijars, the background task apparatus will fail to find task
  // definitions.
  {
    let omniJa = Services.dirsvc.get("XCurProcD", Ci.nsIFile);
    omniJa.append("omni.ja");
    if (!omniJa.exists()) {
      Assert.ok(
        false,
        "This test requires a packaged build, " +
          "run 'mach package' and then use 'mach xpcshell-test --xre-path=...'"
      );
      return;
    }
  }

  if (!setupTestCommon()) {
    return;
  }

  // `channel-prefs.js` is required for Firefox to launch, including in
  // background task mode.  The testing partial MAR updates `channel-prefs.js`
  // to have contents `FromPartial`, which is not a valid prefs file and causes
  // Firefox to crash.  However, `channel-prefs.js` is listed as `add-if-not
  // "defaults/pref/channel-prefs.js" "defaults/pref/channel-prefs.js"`, so it
  // won't be updated if it already exists.  The manipulations below arrange a)
  // for the file to exist and b) for the comparison afterward to succeed.
  gTestFiles = gTestFilesPartialSuccess;
  let channelPrefs = gTestFiles[gTestFiles.length - 1];
  Assert.equal("channel-prefs.js", channelPrefs.fileName);
  let f = gGREDirOrig.clone();
  f.append("defaults");
  f.append("pref");
  f.append("channel-prefs.js");
  // `originalFile` is a relative path, so we can't just point to the one in the
  // original GRE directory.
  channelPrefs.originalFile = null;
  channelPrefs.originalContents = readFile(f);
  channelPrefs.compareContents = channelPrefs.originalContents;
  gTestDirs = gTestDirsPartialSuccess;
  // The third parameter will test that a relative path that contains a
  // directory traversal to the post update binary doesn't execute.
  await setupUpdaterTest(FILE_PARTIAL_MAR, false, "test/../", true, {
    // We need packaged JavaScript to run background tasks.
    requiresOmnijar: true,
  });

  // `0/00/00text2` is just a random file in the testing partial MAR that does
  // not exist before the update and is always written by the update.
  let exitCode;
  exitCode = await do_backgroundtask("file_exists", {
    extraArgs: [getApplyDirFile(DIR_RESOURCES + "0/00/00text2").path],
  });
  // Before updating, file doesn't exist.
  Assert.equal(11, exitCode);

  // This task will wait 10 seconds before exiting, which should overlap with
  // the update below.  We wait for some output from the wait background task,
  // so that there is meaningful overlap.
  let taskStarted = Promise.withResolvers();
  let p = do_backgroundtask("wait", {
    onStdoutLine: (line, proc) => {
      // This sentinel seems pretty safe: it's printed by the task itself and so
      // there should be a straight line between future test failures and
      // logging changes.
      if (line.includes("runBackgroundTask: wait")) {
        taskStarted.resolve(proc);
      }
    },
  });
  let proc = await taskStarted.promise;

  runUpdate(STATE_SUCCEEDED, false, 0, true);

  checkAppBundleModTime();
  standardInit();
  checkPostUpdateRunningFile(false);
  checkFilesAfterUpdateSuccess(getApplyDirFile);
  checkUpdateLogContents(LOG_PARTIAL_SUCCESS);
  await waitForUpdateXMLFiles();
  checkUpdateManager(STATE_NONE, false, STATE_SUCCEEDED, 0, 1);

  // Once we've seen what we want, there's no need to let the task wait complete.
  await proc.kill();

  Assert.ok("Waiting for background task to die after kill()");
  exitCode = await p;

  // Windows does not support killing processes gracefully, so they will
  // always exit with -9 there.
  let retVal = AppConstants.platform == "win" ? -9 : -15;
  Assert.equal(retVal, exitCode);

  exitCode = await do_backgroundtask("file_exists", {
    extraArgs: [getApplyDirFile(DIR_RESOURCES + "0/00/00text2").path],
  });
  // After updating, file exists.
  Assert.equal(0, exitCode);

  // This finishes the test, so must be last.
  checkCallbackLog();
}
