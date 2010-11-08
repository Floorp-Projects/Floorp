/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/* Test applying an update by staging an update and launching an application */

// Use a copy of the main application executable for the test to avoid main
// executable in use errors.
const FILE_WIN_TEST_EXE = "aus_test_app.exe";

// Backup the updater.ini and use a custom one to prevent the updater from
// launching a post update executable.
const FILE_UPDATER_INI_BAK = "updater.ini.bak";

// Number of milliseconds for each do_timeout call.
const CHECK_TIMEOUT_MILLI = 1000;

// Maximum number of milliseconds the process that is launched can run before
// the test will try to kill it.
const APP_TIMER_TIMEOUT = 15000;

let gAppTimer;
let gProcess;

// Environment related globals
let gShouldResetEnv = undefined;
let gAddedEnvXRENoWindowsCrashDialog = false;
let gEnvXPCOMDebugBreak;
let gEnvXPCOMMemLeakLog;
let gEnvDyldLibraryPath;
let gEnvLdLibraryPath;

// A shell script is used to get the OS version due to nsSystemInfo not
// returning the actual OS version. It is possible to get the actual OS version
// using ctypes but it would be more complicated than using a shell script.
XPCOMUtils.defineLazyGetter(this, "gIsLessThanMacOSX_10_6", function test_gMacVer() {
  if (!IS_MACOSX) {
    return false;
  }

  let [versionScript, versionFile] = getVersionScriptAndFile();
  // Precreate the script with executable permissions
  versionScript.create(AUS_Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_DIRECTORY);
  let scriptContents = "#! /bin/sh\nsw_vers -productVersion >> " + versionFile.path;
  writeFile(versionScript, scriptContents);
  logTestInfo("created " + versionScript.path + " shell script containing:\n" +
              scriptContents);

  let versionScriptPath = versionScript.path;
  if (/ /.test(versionScriptPath)) {
    versionScriptPath = '"' + versionScriptPath + '"';
  }


  let launchBin = getLaunchBin();
  let args = [versionScriptPath];
  let process = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  process.init(launchBin);
  process.run(true, args, args.length);
  if (process.exitValue != 0) {
    do_throw("Version script exited with " + process.exitValue + "... unable " +
             "to get Mac OS X version!");
  }

  let version = readFile(versionFile).split("\n")[0];
  logTestInfo("executing on Mac OS X verssion " + version);

  return (Services.vc.compare(version, "10.6") < 0)
});

/**
 * Checks for the existence of a platform specific application binary that can
 * be used for the test and gets its path if it is found.
 *
 * Note: The application shell scripts for launching the application work on all
 * platforms that provide a launch shell script except for Mac OS X 10.5 which
 * is why this test uses the binaries to launch the application.
 */
XPCOMUtils.defineLazyGetter(this, "gAppBinPath", function test_gAppBinPath() {
  let processDir = getCurrentProcessDir();
  let appBin = processDir.clone();
  appBin.append(APP_BIN_NAME + APP_BIN_SUFFIX);
  if (appBin.exists()) {
    if (IS_WIN) {
      let appBinCopy = processDir.clone();
      appBinCopy.append(FILE_WIN_TEST_EXE);
      if (appBinCopy.exists()) {
        appBinCopy.remove(false);
      }
      appBin.copyTo(processDir, FILE_WIN_TEST_EXE);
      appBin = processDir.clone();
      appBin.append(FILE_WIN_TEST_EXE);
    }
    let appBinPath = appBin.path;
    if (/ /.test(appBinPath)) {
      appBinPath = '"' + appBinPath + '"';
    }
    return appBinPath;
  }
  return null;
});

function run_test() {
  if (IS_ANDROID) {
    logTestInfo("this test is not applicable to Android... returning early");
    return;
  }

  if (IS_WINCE) {
    logTestInfo("this test is not applicable to Windows CE... returning early");
    return;
  }

  do_test_pending();
  do_register_cleanup(end_test);

  removeUpdateDirsAndFiles();

  if (!gAppBinPath) {
    do_throw("Main application binary not found... expected: " +
             APP_BIN_NAME + APP_BIN_SUFFIX);
    return;
  }

  let channel = Services.prefs.getCharPref(PREF_APP_UPDATE_CHANNEL);
  let patches = getLocalPatchString(null, null, null, null, null, "true",
                                    STATE_PENDING);
  let updates = getLocalUpdateString(patches, null, null, null, null, null,
                                     null, null, null, null, null, null,
                                     null, "true", channel);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);

  // Read the application.ini and use its application version
  let processDir = getCurrentProcessDir();
  let file = processDir.clone();
  file.append("application.ini");
  let ini = AUS_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
            getService(AUS_Ci.nsIINIParserFactory).
            createINIParser(file);
  let version = ini.getString("App", "Version");
  writeVersionFile(version);
  writeStatusFile(STATE_PENDING);

  // This is the directory where the update files will be located
  let updateTestDir = getUpdateTestDir();
  try {
    removeDirRecursive(updateTestDir);
  }
  catch (e) {
    dump("Unable to remove directory\n" +
         "path: " + updateTestDir.path + "\n" +
         "Exception: " + e + "\n");
  }

  // Add the directory where the update files will be added and add files that
  // will be removed.
  if (!updateTestDir.exists()) {
    updateTestDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  logTestInfo("update test directory path: " + updateTestDir.path);

  file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  writeFile(file, "ToBeRemoved");

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  writeFile(file, "ToBeReplaced");

  file = updateTestDir.clone();
  file.append("removed-files");
  writeFile(file, "ToBeReplaced");

  let updatesPatchDir = getUpdatesDir();
  updatesPatchDir.append("0");
  let mar = do_get_file("data/simple.mar");
  mar.copyTo(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  // Backup the updater.ini
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  updaterIni.moveTo(processDir, FILE_UPDATER_INI_BAK);
  // Create a new updater.ini to avoid applications that provide a post update
  // executable.
  let updaterIniContents = "[Strings]\n" +
                           "Title=Update Test\n" +
                           "Info=Application Update XPCShell Test - " +
                           "test_0200_general.js\n";
  updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI);
  writeFile(updaterIni, updaterIniContents);

  let launchBin = getLaunchBin();
  let args = getProcessArgs();
  let infoArgs = "";
  args.forEach(function(aArg) {
    infoArgs += " " + aArg;
  });
  logTestInfo("launching " + launchBin.path + infoArgs);

  gProcess = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  gProcess.init(launchBin);

  gAppTimer = AUS_Cc["@mozilla.org/timer;1"].createInstance(AUS_Ci.nsITimer);
  gAppTimer.initWithCallback(gTimerCallback, APP_TIMER_TIMEOUT,
                             AUS_Ci.nsITimer.TYPE_ONE_SHOT);

  setEnvironment();

  gProcess.runAsync(args, args.length, gProcessObserver);

  resetEnvironment();
}

function end_test() {
  if (gProcess.isRunning) {
    logTestInfo("attempt to kill process");
    gProcess.kill();
  }

  if (gAppTimer) {
    logTestInfo("cancelling timer");
    gAppTimer.cancel();
    gAppTimer = null;
  }

  resetEnvironment();

  let processDir = getCurrentProcessDir();
  // Restore the backed up updater.ini
  let updaterIni = processDir.clone();
  updaterIni.append(FILE_UPDATER_INI_BAK);
  updaterIni.moveTo(processDir, FILE_UPDATER_INI);

  if (IS_WIN) {
    // Remove the copy of the application executable used for the test on
    // Windows if it exists.
    let appBinCopy = processDir.clone();
    appBinCopy.append(FILE_WIN_TEST_EXE);
    if (appBinCopy.exists()) {
      appBinCopy.remove(false);
    }
  }

  // Remove the files added by the update.
  let updateTestDir = getUpdateTestDir();
  try {
    logTestInfo("removing update test directory " + updateTestDir.path);
    removeDirRecursive(updateTestDir);
  }
  catch (e) {
    dump("Unable to remove directory\n" +
         "path: " + updateTestDir.path + "\n" +
         "Exception: " + e + "\n");
  }

  // This will delete the app console log file if it exists.
  getAppConsoleLogPath();

  if (IS_UNIX) {
    // This will delete the launch script if it exists.
    getLaunchScript();
    if (IS_MACOSX) {
      // This will delete the version script and version file if they exist.
      getVersionScriptAndFile();
    }
  }

  cleanUp();
}

/**
 * The observer for the call to nsIProcess:runAsync.
 */
let gProcessObserver = {
  observe: function PO_observe(subject, topic, data) {
    logTestInfo("topic " + topic + ", process exitValue " + gProcess.exitValue);
    if (gAppTimer) {
      gAppTimer.cancel();
      gAppTimer = null;
    }
    if (topic != "process-finished" || gProcess.exitValue != 0) {
      do_throw("Failed to launch application");
    }
    do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
  },
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsIObserver])
};

/**
 * The timer callback to kill the process if it takes too long.
 */
let gTimerCallback = {
  notify: function TC_notify(aTimer) {
    gAppTimer = null;
    if (gProcess.isRunning) {
      gProcess.kill();
    }
    do_throw("launch application timer expired");
  },
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsITimerCallback])
};

/**
 * Sets the environment that will be used by the application process when it is
 * launched.
 */
function setEnvironment() {
  // Prevent setting the environment more than once.
  if (gShouldResetEnv !== undefined)
    return;

  gShouldResetEnv = true;

  let env = AUS_Cc["@mozilla.org/process/environment;1"].
            getService(AUS_Ci.nsIEnvironment);
  if (IS_WIN && !env.exists("XRE_NO_WINDOWS_CRASH_DIALOG")) {
    gAddedEnvXRENoWindowsCrashDialog = true;
    logTestInfo("setting the XRE_NO_WINDOWS_CRASH_DIALOG environment " +
                "variable to 1... previously it didn't exist");
    env.set("XRE_NO_WINDOWS_CRASH_DIALOG", "1");
  }

  if (IS_UNIX) {
    let appGreDir = Services.dirsvc.get("GreD", AUS_Ci.nsIFile);
    let envGreDir = AUS_Cc["@mozilla.org/file/local;1"].
                    createInstance(AUS_Ci.nsILocalFile);
    let shouldSetEnv = true;
    if (IS_MACOSX) {
      if (env.exists("DYLD_LIBRARY_PATH")) {
        gEnvDyldLibraryPath = env.get("DYLD_LIBRARY_PATH");
        envGreDir.initWithPath(gEnvDyldLibraryPath);
        if (envGreDir.path == appGreDir.path) {
          gEnvDyldLibraryPath = null;
          shouldSetEnv = false;
        }
      }

      if (shouldSetEnv) {
        logTestInfo("setting DYLD_LIBRARY_PATH environment variable value to " +
                    appGreDir.path);
        env.set("DYLD_LIBRARY_PATH", appGreDir.path);
      }
    }
    else {
      if (env.exists("LD_LIBRARY_PATH")) {
        gEnvLdLibraryPath = env.get("LD_LIBRARY_PATH");
        envGreDir.initWithPath(gEnvLdLibraryPath);
        if (envGreDir.path == appGreDir.path) {
          gEnvLdLibraryPath = null;
          shouldSetEnv = false;
        }
      }

      if (shouldSetEnv) {
        logTestInfo("setting LD_LIBRARY_PATH environment variable value to " +
                    appGreDir.path);
        env.set("LD_LIBRARY_PATH", appGreDir.path);
      }
    }
  }

  if (env.exists("XPCOM_MEM_LEAK_LOG")) {
    gEnvXPCOMMemLeakLog = env.get("XPCOM_MEM_LEAK_LOG");
    logTestInfo("removing the XPCOM_MEM_LEAK_LOG environment variable... " +
                "previous value " + gEnvXPCOMMemLeakLog);
    env.set("XPCOM_MEM_LEAK_LOG", "");
  }

  if (env.exists("XPCOM_DEBUG_BREAK")) {
    gEnvXPCOMDebugBreak = env.get("XPCOM_DEBUG_BREAK");
    logTestInfo("setting the XPCOM_DEBUG_BREAK environment variable to " +
                "warn... previous value " + gEnvXPCOMDebugBreak);
  }
  else {
    logTestInfo("setting the XPCOM_DEBUG_BREAK environment variable to " +
                "warn... previously it didn't exist");
  }

  env.set("XPCOM_DEBUG_BREAK", "warn");
}

/**
 * Sets the environment back to the original values after launching the
 * application.
 */
function resetEnvironment() {
  // Prevent resetting the environment more than once.
  if (gShouldResetEnv !== true)
    return;

  gShouldResetEnv = false;

  let env = AUS_Cc["@mozilla.org/process/environment;1"].
            getService(AUS_Ci.nsIEnvironment);

  if (gEnvXPCOMMemLeakLog) {
    logTestInfo("setting the XPCOM_MEM_LEAK_LOG environment variable back to " +
                gEnvXPCOMMemLeakLog);
    env.set("XPCOM_MEM_LEAK_LOG", gEnvXPCOMMemLeakLog);
  }

  if (gEnvXPCOMDebugBreak) {
    logTestInfo("setting the XPCOM_DEBUG_BREAK environment variable back to " +
                gEnvXPCOMDebugBreak);
    env.set("XPCOM_DEBUG_BREAK", gEnvXPCOMDebugBreak);
  }
  else {
    logTestInfo("clearing the XPCOM_DEBUG_BREAK environment variable");
    env.set("XPCOM_DEBUG_BREAK", "");
  }

  if (IS_UNIX) {
    if (IS_MACOSX) {
      if (gEnvDyldLibraryPath) {
        logTestInfo("setting DYLD_LIBRARY_PATH environment variable value " +
                    "back to " + gEnvDyldLibraryPath);
        env.set("DYLD_LIBRARY_PATH", gEnvDyldLibraryPath);
      }
      else {
        logTestInfo("removing DYLD_LIBRARY_PATH environment variable");
        env.set("DYLD_LIBRARY_PATH", "");
      }
    }
    else {
      if (gEnvLdLibraryPath) {
        logTestInfo("setting LD_LIBRARY_PATH environment variable value back " +
                    "to " + gEnvLdLibraryPath);
        env.set("LD_LIBRARY_PATH", gEnvLdLibraryPath);
      }
      else {
        logTestInfo("removing LD_LIBRARY_PATH environment variable");
        env.set("LD_LIBRARY_PATH", "");
      }
    }
  }

  if (IS_WIN && gAddedEnvXRENoWindowsCrashDialog) {
    logTestInfo("removing the XRE_NO_WINDOWS_CRASH_DIALOG environment " +
                "variable");
    env.set("XRE_NO_WINDOWS_CRASH_DIALOG", "");
  }
}

/**
 * Gets the platform specific binary used to launch the application using
 * nsIProcess.
 *
 * @return  nsIFile for the binary to launch using nsIProcess.
 */
function getLaunchBin() {
  let launchBin;
  if (IS_WIN) {
    launchBin = Services.dirsvc.get("WinD", AUS_Ci.nsIFile);
    launchBin.append("System32");
    launchBin.append("cmd.exe");
  }
  else {
    launchBin = AUS_Cc["@mozilla.org/file/local;1"].
                createInstance(AUS_Ci.nsILocalFile);
    launchBin.initWithPath("/bin/sh");
  }
  return launchBin;
}

/**
 * Returns the platform specific arguments used by nsIProcess when launching
 * the application.
 *
 * @return  an array of arguments to be passed to nsIProcess.
 *
 * Notes:
 * 1. Mozilla universal binaries that contain both i386 and x86_64 on Mac OS X
 *    10.5.x must be launched using the i386 architecture.
 * 2. A shell is necessary to pipe the application's console output which
 *    would otherwise pollute the xpcshell log.
 *
 * Command line arguments used when launching the application:
 * -no-remote prevents shell integration from being affected by an existing
 * application process.
 * -process-updates makes the application exits after being relaunched by the
 * updater.
 * 1> pipes stdout to a file.
 * appConsoleLogPath is the file path to pipe the output from the shell.
 * Otherwise the output from the application will end up in the xpchsell log.
 * 2>&1 pipes stderr to sdout.
 */
function getProcessArgs() {
  // Pipe the output from the launched application to a file so the output from
  // its console isn't present in the xpcshell log.
  let appConsoleLogPath = getAppConsoleLogPath();

  let args;
  if (IS_UNIX) {
    let launchScript = getLaunchScript();
    // Precreate the script with executable permissions
    launchScript.create(AUS_Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_DIRECTORY);

    let scriptContents = "#! /bin/sh\n";
    // On Mac OS X versions prior to 10.6 the i386 acrhitecture must be used.
    if (gIsLessThanMacOSX_10_6) {
      scriptContents += "arch -arch i386 ";
    }
    scriptContents += gAppBinPath + " -no-remote -process-updates 1> " +
                      appConsoleLogPath + " 2>&1";
    writeFile(launchScript, scriptContents);
    logTestInfo("created " + launchScript.path + " containing:\n" +
                scriptContents);
    args = [launchScript.path];
  }
  else {
    args = ["/D", "/Q", "/C", gAppBinPath, "-no-remote", "-process-updates",
            "1>", appConsoleLogPath, "2>&1"];
  }
  return args;
}

/**
 * Gets the directory where the update adds / removes the files contained in the
 * update.
 *
 * @return  nsIFile for the directory where the update adds / removes the files
 *          contained in the update mar.
 */
function getUpdateTestDir() {
  let updateTestDir = getCurrentProcessDir();
  if (IS_MACOSX) {
    updateTestDir = updateTestDir.parent.parent;
  }
  updateTestDir.append("update_test");
  return updateTestDir;
}

/**
 * Gets a file path for piping the console output from the application so it
 * doesn't appear in the xpcshell log file.
 *
 * @return  path to the file for piping the console output from the application.
 */
function getAppConsoleLogPath() {
  let appConsoleLog = do_get_file("/", true);
  appConsoleLog.append("app_console_log");
  if (appConsoleLog.exists()) {
    appConsoleLog.remove(false);
  }
  let appConsoleLogPath = appConsoleLog.path;
  if (/ /.test(appConsoleLogPath)) {
    appConsoleLogPath = '"' + appConsoleLogPath + '"';
  }
  return appConsoleLogPath;
}

/**
 * Gets the nsIFile references for the shell script to retrieve the Mac OS X
 * version and the nsIFile to pipe the output of the shell script. If either of
 * these files exist they will be removed by this function.
 *
 * @return  array containing two nsIFile references. The first array member is
 *          the nsIFile for the shell script to launch to get the Mac OS X
 *          version and the second array member is the nsIFile for the piped
 *          output from the shell script.
 */
function getVersionScriptAndFile() {
  let versionScript = do_get_file("/", true);
  let versionFile = versionScript.clone();
  versionScript.append("get_version.sh");
  if (versionScript.exists()) {
    versionScript.remove(false);
  }
  versionFile.append("version.out");
  if (versionFile.exists()) {
    versionFile.remove(false);
  }
  return [versionScript, versionFile];
}

/**
 * Gets the nsIFile reference for the shell script to launch the application. If
 * the file exists it will be removed by this function.
 *
 * @return  the nsIFile for the shell script to launch the application.
 */
function getLaunchScript() {
  let launchScript = do_get_file("/", true);
  launchScript.append("launch.sh");
  if (launchScript.exists()) {
    launchScript.remove(false);
  }
  return launchScript;
}

/**
 * Checks if the update has finished and if it has finished performs checks for
 * the test.
 */
function checkUpdateFinished() {
  // Don't proceed until the update.log has been created.
  let log = getUpdatesDir();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  if (!log.exists()) {
    do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
    return;
  }

  // Don't proceed until the update status is no longer pending or applying.
  let status = readStatusFile();
  if (status == STATE_PENDING || status == STATE_APPLYING) {
    do_timeout(CHECK_TIMEOUT_MILLI, checkUpdateFinished);
    return;
  }

  // Log the contents of the update.log so it is simpler to diagnose a test
  // failure. For example, on Windows if the application binary is in use the
  // updater will not apply the update.
  let contents = readFile(log);
  logTestInfo("contents of " + log.path + ":\n" +  
              contents.replace(/\r\n/g, "\n"));

  if (IS_WIN && contents.indexOf("NS_main: file in use") != -1) {
    do_throw("the application can't be in use when running this test");
  }

  do_check_eq(status, STATE_SUCCEEDED);

  standardInit();

  let update = gUpdateManager.getUpdateAt(0);
  do_check_eq(update.state, STATE_SUCCEEDED);

  let updateTestDir = getUpdateTestDir();

  let file = updateTestDir.clone();
  file.append("UpdateTestRemoveFile");
  do_check_false(file.exists());

  file = updateTestDir.clone();
  file.append("UpdateTestAddFile");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "UpdateTestAddFile\n");

  file = updateTestDir.clone();
  file.append("removed-files");
  do_check_true(file.exists());
  do_check_eq(readFileBytes(file), "update_test/UpdateTestRemoveFile\n");

  let updatesDir = getUpdatesDir();
  let log = updatesDir.clone();
  log.append("0");
  log.append(FILE_UPDATE_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  log = updatesDir.clone();
  log.append(FILE_LAST_LOG);
  logTestInfo("testing " + log.path + " should exist");
  do_check_true(log.exists());

  log = updatesDir.clone();
  log.append(FILE_BACKUP_LOG);
  logTestInfo("testing " + log.path + " shouldn't exist");
  do_check_false(log.exists());

  updatesDir.append("0");
  logTestInfo("testing " + updatesDir.path + " should exist");
  do_check_true(updatesDir.exists());

  do_timeout(CHECK_TIMEOUT_MILLI, do_test_finished);
}
