/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const INSTALL_LOCALE = "@AB_CD@";
const MOZ_APP_NAME = "@MOZ_APP_NAME@";
const BIN_SUFFIX = "@BIN_SUFFIX@";

#ifdef XP_WIN
// MOZ_APP_VENDOR is optional.
// On Windows, if MOZ_APP_VENDOR is not defined the updates directory will be
// located under %LOCALAPPDATA%\@MOZ_APP_BASENAME@\updates\TaskBarID
#ifdef MOZ_APP_VENDOR
const MOZ_APP_VENDOR = "@MOZ_APP_VENDOR@";
#else
const MOZ_APP_VENDOR = "";
#endif

// MOZ_APP_BASENAME is not optional for tests.
const MOZ_APP_BASENAME = "@MOZ_APP_BASENAME@";
#endif // XP_WIN

const APP_INFO_NAME = "XPCShell";
const APP_INFO_VENDOR = "Mozilla";

#ifdef XP_UNIX
const APP_BIN_SUFFIX = "-bin";
#else
const APP_BIN_SUFFIX = "@BIN_SUFFIX@";
#endif

#ifdef XP_WIN
const IS_WIN = true;
#else
const IS_WIN = false;
#endif

#ifdef XP_OS2
const IS_OS2 = true;
#else
const IS_OS2 = false;
#endif

#ifdef XP_MACOSX
const IS_MACOSX = true;
#ifdef MOZ_SHARK
const IS_SHARK = true;
#else
const IS_SHARK = false;
#endif
#else
const IS_MACOSX = false;
#endif

#ifdef XP_UNIX
const IS_UNIX = true;
#else
const IS_UNIX = false;
#endif

#ifdef ANDROID
const IS_ANDROID = true;
#else
const IS_ANDROID = false;
#endif

#ifdef MOZ_WIDGET_GONK
const IS_TOOLKIT_GONK = true;
#else
const IS_TOOLKIT_GONK = false;
#endif

const USE_EXECV = IS_UNIX && !IS_MACOSX;

#ifdef MOZ_VERIFY_MAR_SIGNATURE
const IS_MAR_CHECKS_ENABLED = true;
#else
const IS_MAR_CHECKS_ENABLED = false;
#endif

const URL_HOST = "http://localhost";

const FILE_APP_BIN = MOZ_APP_NAME + APP_BIN_SUFFIX;
const FILE_COMPLETE_MAR = "complete.mar";
const FILE_COMPLETE_WIN_MAR = "complete_win.mar";
const FILE_HELPER_BIN = "TestAUSHelper" + BIN_SUFFIX;
const FILE_MAINTENANCE_SERVICE_BIN = "maintenanceservice.exe";
const FILE_MAINTENANCE_SERVICE_INSTALLER_BIN = "maintenanceservice_installer.exe";
const FILE_OLD_VERSION_MAR = "old_version.mar";
const FILE_PARTIAL_MAR = "partial.mar";
const FILE_PARTIAL_WIN_MAR = "partial_win.mar";
const FILE_UPDATER_BIN = "updater" + BIN_SUFFIX;
const FILE_UPDATER_INI_BAK = "updater.ini.bak";
const FILE_WRONG_CHANNEL_MAR = "wrong_product_channel.mar";

const LOG_COMPLETE_SUCCESS = "complete_log_success";
const LOG_COMPLETE_SWITCH_SUCCESS = "complete_log_switch_success"
const LOG_COMPLETE_CC_SUCCESS = "complete_cc_log_success";
const LOG_COMPLETE_CC_SWITCH_SUCCESS = "complete_cc_log_switch_success";

const LOG_PARTIAL_SUCCESS = "partial_log_success";
const LOG_PARTIAL_SWITCH_SUCCESS = "partial_log_switch_success";
const LOG_PARTIAL_FAILURE = "partial_log_failure";

const ERR_RENAME_FILE = "rename_file: failed to rename file";
const ERR_UNABLE_OPEN_DEST = "unable to open destination file";
const ERR_BACKUP_DISCARD = "backup_discard: unable to remove";

const LOG_SVC_SUCCESSFUL_LAUNCH = "Process was started... waiting on result.";

// All we care about is that the last modified time has changed so that Mac OS
// X Launch Services invalidates its cache so the test allows up to one minute
// difference in the last modified time.
const MAC_MAX_TIME_DIFFERENCE = 60000;

// Time to wait for the test helper process before continuing the test
const TEST_HELPER_TIMEOUT = 100;

// Time to wait for a check in the test before continuing the test
const TEST_CHECK_TIMEOUT = 100;

// How many of TEST_CHECK_TIMEOUT to wait before we abort the test.
const MAX_TIMEOUT_RUNS = 2000;

// Time in seconds the helper application should sleep.the helper's input and output files
const HELPER_SLEEP_TIMEOUT = 180;

// Maximum number of milliseconds the process that is launched can run before
// the test will try to kill it.
const APP_TIMER_TIMEOUT = 120000;

#ifdef XP_WIN
const PIPE_TO_NULL = ">nul";
#else
const PIPE_TO_NULL = "> /dev/null 2>&1";
#endif

// This default value will be overridden when using the http server.
var gURLData = URL_HOST + "/";

var gTestID;

var gTestserver;

var gRegisteredServiceCleanup;

var gXHR;
var gXHRCallback;

var gUpdatePrompt;
var gUpdatePromptCallback;

var gCheckFunc;
var gResponseBody;
var gResponseStatusCode = 200;
var gRequestURL;
var gUpdateCount;
var gUpdates;
var gStatusCode;
var gStatusText;
var gStatusResult;

var gProcess;
var gAppTimer;
var gHandle;

var gGREDirOrig;
var gAppDirOrig;

var gServiceLaunchedCallbackLog = null;
var gServiceLaunchedCallbackArgs = null;

// Variables are used instead of contants so tests can override these values if
// necessary.
var gCallbackBinFile = "callback_app" + BIN_SUFFIX;
var gCallbackArgs = ["./", "callback.log", "Test Arg 2", "Test Arg 3"];
var gStageUpdate = false;
var gSwitchApp = false;
var gDisableReplaceFallback = false;
var gUseTestAppDir = true;

var gTimeoutRuns = 0;

// Environment related globals
var gShouldResetEnv = undefined;
var gAddedEnvXRENoWindowsCrashDialog = false;
var gEnvXPCOMDebugBreak;
var gEnvXPCOMMemLeakLog;
var gEnvDyldLibraryPath;
var gEnvLdLibraryPath;

/**
 * The mar files used for the updater tests contain the following remove
 * operations.
 *
 * partial and complete test mar remove operations
 * -----------------------------------------------
 * remove "text1"
 * remove "text0"
 * rmrfdir "9/99/"
 * rmdir "9/99/"
 * rmrfdir "9/98/"
 * rmrfdir "9/97/"
 * rmrfdir "9/96/"
 * rmrfdir "9/95/"
 * rmrfdir "9/95/"
 * rmrfdir "9/94/"
 * rmdir "9/94/"
 * rmdir "9/93/"
 * rmdir "9/92/"
 * rmdir "9/91/"
 * rmdir "9/90/"
 * rmdir "9/90/"
 * rmrfdir "8/89/"
 * rmdir "8/89/"
 * rmrfdir "8/88/"
 * rmrfdir "8/87/"
 * rmrfdir "8/86/"
 * rmrfdir "8/85/"
 * rmrfdir "8/85/"
 * rmrfdir "8/84/"
 * rmdir "8/84/"
 * rmdir "8/83/"
 * rmdir "8/82/"
 * rmdir "8/81/"
 * rmdir "8/80/"
 * rmdir "8/80/"
 * rmrfdir "7/"
 * rmdir "6/"
 * remove "5/text1"
 * remove "5/text0"
 * rmrfdir "5/"
 * remove "4/text1"
 * remove "4/text0"
 * remove "4/exe0.exe"
 * rmdir "4/"
 * remove "3/text1"
 * remove "3/text0"
 *
 * partial test mar additional remove operations
 * ---------------------------------------------
 * remove "0/00/00text1"
 * remove "1/10/10text0"
 * rmdir "1/10/"
 * rmdir "1/"
 */
var TEST_DIRS = [
{
  relPathDir   : "a/b/3/",
  dirRemoved   : false,
  files        : ["3text0", "3text1"],
  filesRemoved : true
}, {
  relPathDir   : "a/b/4/",
  dirRemoved   : true,
  files        : ["4text0", "4text1"],
  filesRemoved : true
}, {
  relPathDir   : "a/b/5/",
  dirRemoved   : true,
  files        : ["5test.exe", "5text0", "5text1"],
  filesRemoved : true
}, {
  relPathDir   : "a/b/6/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/7/",
  dirRemoved   : true,
  files        : ["7text0", "7text1"],
  subDirs      : ["70/", "71/"],
  subDirFiles  : ["7xtest.exe", "7xtext0", "7xtext1"]
}, {
  relPathDir   : "a/b/8/",
  dirRemoved   : false
}, {
  relPathDir   : "a/b/8/80/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/8/81/",
  dirRemoved   : false,
  files        : ["81text0", "81text1"]
}, {
  relPathDir   : "a/b/8/82/",
  dirRemoved   : false,
  subDirs      : ["820/", "821/"]
}, {
  relPathDir   : "a/b/8/83/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/8/84/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/8/85/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/8/86/",
  dirRemoved   : true,
  files        : ["86text0", "86text1"]
}, {
  relPathDir   : "a/b/8/87/",
  dirRemoved   : true,
  subDirs      : ["870/", "871/"],
  subDirFiles  : ["87xtext0", "87xtext1"]
}, {
  relPathDir   : "a/b/8/88/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/8/89/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/9/90/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/9/91/",
  dirRemoved   : false,
  files        : ["91text0", "91text1"]
}, {
  relPathDir   : "a/b/9/92/",
  dirRemoved   : false,
  subDirs      : ["920/", "921/"]
}, {
  relPathDir   : "a/b/9/93/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/9/94/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/9/95/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/9/96/",
  dirRemoved   : true,
  files        : ["96text0", "96text1"]
}, {
  relPathDir   : "a/b/9/97/",
  dirRemoved   : true,
  subDirs      : ["970/", "971/"],
  subDirFiles  : ["97xtext0", "97xtext1"]
}, {
  relPathDir   : "a/b/9/98/",
  dirRemoved   : true
}, {
  relPathDir   : "a/b/9/99/",
  dirRemoved   : true
}];

// Populated by tests if needed.
var ADDITIONAL_TEST_DIRS = [];

// Set to true to log additional information for debugging. To log additional
// information for an individual test set DEBUG_AUS_TEST to true in the test's
// run_test function.
var DEBUG_AUS_TEST = true;
// Never set DEBUG_TEST_LOG to true except when running tests locally or on the
// try server since this will force a test that failed a parallel run to fail
// when the same test runs non-parallel so the log from parallel test run can
// be displayed in the log.
var DEBUG_TEST_LOG = false;
// Set to false to keep the log file from the failed parallel test run.
var gDeleteLogFile = true;
var gRealDump;
var gTestLogText = "";
var gPassed;

#include ../shared.js

// This makes it possible to run most tests on xulrunner where the update
// channel default preference is not set.
if (MOZ_APP_NAME == "xulrunner") {
  try {
    gDefaultPrefBranch.getCharPref(PREF_APP_UPDATE_CHANNEL);
  } catch (e) {
    setUpdateChannel("test_channel");
  }
}

/**
 * Helper function for setting up the test environment.
 */
function setupTestCommon() {
  logTestInfo("start - general test setup");

  do_test_pending();

  if (gTestID) {
    do_throw("setupTestCommon should only be called once!");
  }

  let caller = Components.stack.caller;
  gTestID = caller.filename.toString().split("/").pop().split(".")[0];

  if (DEBUG_TEST_LOG) {
    let logFile = do_get_file(gTestID + ".log", true);
    if (logFile.exists()) {
      gPassed = false;
      logTestInfo("start - dumping previous test run log");
      logTestInfo("\n" + readFile(logFile) + "\n");
      logTestInfo("finish - dumping previous test run log");
      if (gDeleteLogFile) {
        logFile.remove(false);
      }
      do_throw("The parallel run of this test failed. Failing non-parallel " +
               "test so the log from the parallel run can be displayed in " +
               "non-parallel log.")
    } else {
      gRealDump = dump;
      dump = dumpOverride;
    }
  }

  // Don't attempt to show a prompt when an update finishes.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, true);

  gGREDirOrig = getGREDir();
  gAppDirOrig = getAppBaseDir();

  let applyDir = getApplyDirFile(null, true).parent;

  // Try to remove the directory used to apply updates and the updates directory
  // on platforms other than Windows. Since the test hasn't ran yet and the
  // directory shouldn't exist finished this is non-fatal for the test.
  if (applyDir.exists()) {
    logTestInfo("attempting to remove directory. Path: " + applyDir.path);
    try {
      removeDirRecursive(applyDir);
    } catch (e) {
      logTestInfo("non-fatal error removing directory. Path: " +
                  applyDir.path + ", Exception: " + e);
    }
  }

  // adjustGeneralPaths registers a cleanup function that calls end_test when
  // it is defined as a function.
  adjustGeneralPaths();

  // Remove the updates directory on Windows which is located outside of the
  // application directory after the call to adjustGeneralPaths has set it up.
  // Since the test hasn't ran yet and the directory shouldn't exist finished
  // this is non-fatal for the test.
  if (IS_WIN) {
    let updatesDir = getMockUpdRootD();
    if (updatesDir.exists())  {
      logTestInfo("attempting to remove directory. Path: " + updatesDir.path);
      try {
        removeDirRecursive(updatesDir);
      } catch (e) {
        logTestInfo("non-fatal error removing directory. Path: " +
                    updatesDir.path + ", Exception: " + e);
      }
    }
  }

  logTestInfo("finish - general test setup");
}

/**
 * Nulls out the most commonly used global vars used by tests to prevent leaks
 * as needed and attempts to restore the system to its original state.
 */
function cleanupTestCommon() {
  logTestInfo("start - general test cleanup");

  // Force the update manager to reload the update data to prevent it from
  // writing the old data to the files that have just been removed.
  reloadUpdateManagerData();

  if (gChannel) {
    gPrefRoot.removeObserver(PREF_APP_UPDATE_CHANNEL, observer);
  }

  // Call app update's observe method passing xpcom-shutdown to test that the
  // shutdown of app update runs without throwing or leaking. The observer
  // method is used directly instead of calling notifyObservers so components
  // outside of the scope of this test don't assert and thereby cause app update
  // tests to fail.
  gAUS.observe(null, "xpcom-shutdown", "");

  if (gXHR) {
    gXHRCallback     = null;

    gXHR.responseXML = null;
    // null out the event handlers to prevent a mFreeCount leak of 1
    gXHR.onerror     = null;
    gXHR.onload      = null;
    gXHR.onprogress  = null;

    gXHR             = null;
  }

  gTestserver = null;

  if (IS_UNIX) {
    // This will delete the launch script if it exists.
    getLaunchScript();
  }

  if (IS_WIN && MOZ_APP_BASENAME) {
    let appDir = getApplyDirFile(null, true);
    let vendor = MOZ_APP_VENDOR ? MOZ_APP_VENDOR : "Mozilla";
    const REG_PATH = "SOFTWARE\\" + vendor + "\\" + MOZ_APP_BASENAME +
                     "\\TaskBarIDs";
    let key = AUS_Cc["@mozilla.org/windows-registry-key;1"].
              createInstance(AUS_Ci.nsIWindowsRegKey);
    try {
      key.open(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, REG_PATH,
               AUS_Ci.nsIWindowsRegKey.ACCESS_ALL);
      if (key.hasValue(appDir.path)) {
        key.removeValue(appDir.path);
      }
    } catch (e) {
    }
    try {
      key.open(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, REG_PATH,
               AUS_Ci.nsIWindowsRegKey.ACCESS_ALL);
      if (key.hasValue(appDir.path)) {
        key.removeValue(appDir.path);
      }
    } catch (e) {
    }
  }

  // The updates directory is located outside of the application directory on
  // Windows so it also needs to be removed.
  if (IS_WIN) {
    let updatesDir = getMockUpdRootD();
    // Try to remove the directory used to apply updates. Since the test has
    // already finished this is non-fatal for the test.
    if (updatesDir.exists()) {
      logTestInfo("attempting to remove directory. Path: " + updatesDir.path);
      try {
        removeDirRecursive(updatesDir);
      } catch (e) {
        logTestInfo("non-fatal error removing directory. Path: " +
                    updatesDir.path + ", Exception: " + e);
      }
    }
  }

  let applyDir = getApplyDirFile(null, true).parent;

  // Try to remove the directory used to apply updates. Since the test has
  // already finished this is non-fatal for the test.
  if (applyDir.exists()) {
    logTestInfo("attempting to remove directory. Path: " + applyDir.path);
    try {
      removeDirRecursive(applyDir);
    } catch (e) {
      logTestInfo("non-fatal error removing directory. Path: " +
                  applyDir.path + ", Exception: " + e);
    }
  }

  resetEnvironment();

  logTestInfo("finish - general test cleanup");

  if (gRealDump) {
    dump = gRealDump;
    gRealDump = null;
  }

  if (DEBUG_TEST_LOG && !gPassed) {
    let fos = AUS_Cc["@mozilla.org/network/file-output-stream;1"].
              createInstance(AUS_Ci.nsIFileOutputStream);
    let logFile = do_get_file(gTestID + ".log", true);
    if (!logFile.exists()) {
      logFile.create(AUS_Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
    }
    fos.init(logFile, MODE_WRONLY | MODE_CREATE | MODE_APPEND, PERMS_FILE, 0);
    fos.write(gTestLogText, gTestLogText.length);
    fos.close();
  }

  if (DEBUG_TEST_LOG) {
    gTestLogText = null;
  } else {
    let logFile = do_get_file(gTestID + ".log", true);
    if (logFile.exists()) {
      logFile.remove(false);
    }
  }
}

/**
 * Helper function to store the log output of calls to dump in a variable so the
 * values can be written to a file for a parallel run of a test and printed to
 * the log file when the test runs synchronously.
 */
function dumpOverride(aText) {
  gTestLogText += aText;
  gRealDump(aText);
}

/**
 * Helper function that calls do_test_finished that tracks whether a parallel
 * run of a test passed when it runs synchronously so the log output can be
 * inspected.
 */
function doTestFinish() {
  if (gPassed === undefined) {
    gPassed = true;
  }
  do_test_finished();
}

/**
 * Sets the most commonly used preferences used by tests
 */
function setDefaultPrefs() {
  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, true);
  Services.prefs.setBoolPref(PREF_APP_UPDATE_METRO_ENABLED, true);
  // Don't display UI for a successful installation. Some apps may not set this
  // pref to false like Firefox does.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SHOW_INSTALLED_UI, false);
  // Enable Update logging
  Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG, true);
}

/**
 * Initializes the most commonly used settings and creates an instance of the
 * update service stub.
 */
function standardInit() {
  createAppInfo("xpcshell@tests.mozilla.org", APP_INFO_NAME, "1.0", "2.0");
  setDefaultPrefs();
  // Initialize the update service stub component
  initUpdateServiceStub();
}

/**
 * Custom path handler for the http server
 *
 * @param   aMetadata
 *          The http metadata for the request.
 * @param   aResponse
 *          The http response for the request.
 */
function pathHandler(aMetadata, aResponse) {
  aResponse.setHeader("Content-Type", "text/xml", false);
  aResponse.setStatusLine(aMetadata.httpVersion, gResponseStatusCode, "OK");
  aResponse.bodyOutputStream.write(gResponseBody, gResponseBody.length);
}

/**
 * Helper function for getting the application version from the application.ini
 * file. This will look in both the GRE and the application directories for the
 * application.ini file.
 *
 * @return  The version string from the application.ini file.
 * @throws  If the application.ini file is not found.
 */
function getAppVersion() {
  // Read the application.ini and use its application version.
  let iniFile = gGREDirOrig.clone();
  iniFile.append("application.ini");
  if (!iniFile.exists()) {
    iniFile = gAppDirOrig.clone();
    iniFile.append("application.ini");
  }
  if (!iniFile.exists()) {
    do_throw("Unable to find application.ini!");
  }
  let iniParser = AUS_Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                  getService(AUS_Ci.nsIINIParserFactory).
                  createINIParser(iniFile);
  return iniParser.getString("App", "Version");
}

/**
 * Helper function for getting the relative path to the directory where the
 * application binary is located (e.g. <test_file_leafname>/dir.app/).
 *
 * Note: The dir.app subdirectory under <test_file_leafname> is needed for
 *       platforms other than Mac OS X so the tests can run in parallel due to
 *       update staging creating a lock file named moz_update_in_progress.lock in
 *       the parent directory of the installation directory.
 *
 * @return  The relative path to the directory where application binary is
 *          located.
 */
function getApplyDirPath() {
  return gTestID + "/dir.app/";
}

/**
 * Helper function for getting the nsIFile for a file in the directory where the
 * update will be applied.
 *
 * The files for the update are located two directories below the apply to
 * directory since Mac OS X sets the last modified time for the root directory
 * to the current time and if the update changes any files in the root directory
 * then it wouldn't be possible to test (bug 600098).
 *
 * @param   aRelPath (optional)
 *          The relative path to the file or directory to get from the root of
 *          the test's directory. If not specified the test's directory will be
 *          returned.
 * @param   aAllowNonexistent (optional)
 *          Whether the file must exist. If false or not specified the file must
 *          exist or the function will throw.
 * @return  The nsIFile for the file in the directory where the update will be
 *          applied.
 */
function getApplyDirFile(aRelPath, aAllowNonexistent) {
  let relpath = getApplyDirPath() + (aRelPath ? aRelPath : "");
  return do_get_file(relpath, aAllowNonexistent);
}

/**
 * Helper function for getting the relative path to the directory where the
 * test data files are located.
 *
 * @return  The relative path to the directory where the test data files are
 *          located.
 */
function getTestDirPath() {
  return "../data/";
}

/**
 * Helper function for getting the nsIFile for a file in the test data
 * directory.
 *
 * @param   aRelPath (optional)
 *          The relative path to the file or directory to get from the root of
 *          the test's data directory. If not specified the test's data
 *          directory will be returned.
 * @return  The nsIFile for the file in the test data directory.
 * @throws  If the file or directory does not exist.
 */
function getTestDirFile(aRelPath) {
  let relpath = getTestDirPath() + (aRelPath ? aRelPath : "");
  return do_get_file(relpath, false);
}

/**
 * Helper function for getting the directory that was updated. This can either
 * be the directory where the application binary is located or the directory
 * that contains the staged update.
 */
function getUpdatedDirPath() {
  return getApplyDirPath() + (gStageUpdate ? DIR_UPDATED +  "/" : "");
}

/**
 * Helper function for getting the directory where files are added, removed,
 * and modified by the simple.mar update file.
 *
 * @return  nsIFile for the directory where files are added, removed, and
 *          modified by the simple.mar update file.
 */
function getUpdateTestDir() {
  return getApplyDirFile("update_test", true);
}

/**
 * Helper function for getting the updating directory which is used by the
 * updater to extract the update manifest and patch files.
 *
 * @return  nsIFile for the directory for the updating directory.
 */
function getUpdatingDir() {
  return getApplyDirFile("updating", true);
}

#ifdef XP_WIN
XPCOMUtils.defineLazyGetter(this, "gInstallDirPathHash",
                            function test_gInstallDirPathHash() {
  // Figure out where we should check for a cached hash value
  if (!MOZ_APP_BASENAME)
    return null;

  let vendor = MOZ_APP_VENDOR ? MOZ_APP_VENDOR : "Mozilla";
  let appDir = getApplyDirFile(null, true);

  const REG_PATH = "SOFTWARE\\" + vendor + "\\" + MOZ_APP_BASENAME +
                   "\\TaskBarIDs";
  let regKey = AUS_Cc["@mozilla.org/windows-registry-key;1"].
               createInstance(AUS_Ci.nsIWindowsRegKey);
  try {
    regKey.open(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, REG_PATH,
                AUS_Ci.nsIWindowsRegKey.ACCESS_ALL);
    regKey.writeStringValue(appDir.path, gTestID);
    return gTestID;
  } catch (e) {
  }

  try {
    regKey.create(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, REG_PATH,
                  AUS_Ci.nsIWindowsRegKey.ACCESS_ALL);
    regKey.writeStringValue(appDir.path, gTestID);
    return gTestID;
  } catch (e) {
    logTestInfo("failed to create registry key. Registry Path: " + REG_PATH +
                ", Key Name: " + appDir.path + ", Key Value: " + gTestID +
                ", Exception " + e);
  }
  return null;
});

XPCOMUtils.defineLazyGetter(this, "gLocalAppDataDir",
                            function test_gLocalAppDataDir() {
  const CSIDL_LOCAL_APPDATA = 0x1c;

  AUS_Cu.import("resource://gre/modules/ctypes.jsm");
  let lib = ctypes.open("shell32");
  let SHGetSpecialFolderPath = lib.declare("SHGetSpecialFolderPathW",
                                           ctypes.winapi_abi,
                                           ctypes.bool, /* bool(return) */
                                           ctypes.int32_t, /* HWND hwndOwner */
                                           ctypes.jschar.ptr, /* LPTSTR lpszPath */
                                           ctypes.int32_t, /* int csidl */
                                           ctypes.bool /* BOOL fCreate */);

  let aryPathLocalAppData = ctypes.jschar.array()(260);
  let rv = SHGetSpecialFolderPath(0, aryPathLocalAppData, CSIDL_LOCAL_APPDATA, false);
  lib.close();

  let pathLocalAppData = aryPathLocalAppData.readString(); // Convert the c-string to js-string
  let updatesDir = AUS_Cc["@mozilla.org/file/local;1"].
                   createInstance(AUS_Ci.nsILocalFile);
  updatesDir.initWithPath(pathLocalAppData);
  return updatesDir;
});

XPCOMUtils.defineLazyGetter(this, "gProgFilesDir",
                            function test_gProgFilesDir() {
  const CSIDL_PROGRAM_FILES = 0x26;

  AUS_Cu.import("resource://gre/modules/ctypes.jsm");
  let lib = ctypes.open("shell32");
  let SHGetSpecialFolderPath = lib.declare("SHGetSpecialFolderPathW",
                                           ctypes.winapi_abi,
                                           ctypes.bool, /* bool(return) */
                                           ctypes.int32_t, /* HWND hwndOwner */
                                           ctypes.jschar.ptr, /* LPTSTR lpszPath */
                                           ctypes.int32_t, /* int csidl */
                                           ctypes.bool /* BOOL fCreate */);

  let aryPathProgFiles = ctypes.jschar.array()(260);
  let rv = SHGetSpecialFolderPath(0, aryPathProgFiles, CSIDL_PROGRAM_FILES, false);
  lib.close();

  let pathProgFiles = aryPathProgFiles.readString(); // Convert the c-string to js-string
  let progFilesDir = AUS_Cc["@mozilla.org/file/local;1"].
                     createInstance(AUS_Ci.nsILocalFile);
  progFilesDir.initWithPath(pathProgFiles);
  return progFilesDir;
});

/**
 * Helper function for getting the update root directory used by the tests. This
 * returns the same directory as returned by nsXREDirProvider::GetUpdateRootDir
 * in nsXREDirProvider.cpp so an application will be able to find the update
 * when running a test that launches the application.
 */
function getMockUpdRootD() {
  let localAppDataDir = gLocalAppDataDir.clone();
  let progFilesDir = gProgFilesDir.clone();
  let appDir = Services.dirsvc.get(XRE_EXECUTABLE_FILE, AUS_Ci.nsIFile).parent;

  let appDirPath = appDir.path;
  var relPathUpdates = "";
  if (gInstallDirPathHash && (MOZ_APP_VENDOR || MOZ_APP_BASENAME)) {
    relPathUpdates += (MOZ_APP_VENDOR ? MOZ_APP_VENDOR : MOZ_APP_BASENAME) +
                      "\\" + DIR_UPDATES + "\\" + gInstallDirPathHash;
  }

  if (!relPathUpdates) {
    if (appDirPath.length > progFilesDir.path.length) {
      if (appDirPath.substr(0, progFilesDir.path.length) == progFilesDir.path) {
        if (MOZ_APP_VENDOR && MOZ_APP_BASENAME) {
          relPathUpdates += MOZ_APP_VENDOR + "\\" + MOZ_APP_BASENAME;
        } else {
          relPathUpdates += MOZ_APP_BASENAME;
        }
        relPathUpdates += appDirPath.substr(progFilesDir.path.length);
      }
    }
  }

  if (!relPathUpdates) {
    if (MOZ_APP_VENDOR && MOZ_APP_BASENAME) {
      relPathUpdates += MOZ_APP_VENDOR + "\\" + MOZ_APP_BASENAME;
    } else {
      relPathUpdates += MOZ_APP_BASENAME;
    }
    relPathUpdates += "\\" + MOZ_APP_NAME;
  }

  var updatesDir = AUS_Cc["@mozilla.org/file/local;1"].
                   createInstance(AUS_Ci.nsILocalFile);
  updatesDir.initWithPath(localAppDataDir.path + "\\" + relPathUpdates);
  logTestInfo("returning UpdRootD Path: " + updatesDir.path);
  return updatesDir;
}
#else
/**
 * Helper function for getting the update root directory used by the tests. This
 * returns the same directory as returned by nsXREDirProvider::GetUpdateRootDir
 * in nsXREDirProvider.cpp so an application will be able to find the update
 * when running a test that launches the application.
 */
function getMockUpdRootD() {
  return getApplyDirFile(DIR_BIN_REL_PATH, true);
}
#endif

/**
 * Helper function for getting the nsIFile for the directory where the update
 * has been applied.
 *
 * This will be the same as getApplyDirFile for foreground updates, but will
 * point to a different file for the case of staged updates.
 *
 * Functions which attempt to access the files in the updated directory should
 * be using this instead of getApplyDirFile.
 *
 * @param   aRelPath (optional)
 *          The relative path to the file or directory to get from the root of
 *          the test's directory. If not specified the test's directory will be
 *          returned.
 * @param   aAllowNonexistent (optional)
 *          Whether the file must exist. If false or not specified the file must
 *          exist or the function will throw.
 * @return  The nsIFile for the directory where the update has been applied.
 */
function getTargetDirFile(aRelPath, aAllowNonexistent) {
  let relpath = getUpdatedDirPath() + (aRelPath ? aRelPath : "");
  return do_get_file(relpath, aAllowNonexistent);
}

if (IS_WIN) {
  const kLockFileName = "updated.update_in_progress.lock";
  /**
   * Helper function for locking a directory on Windows.
   *
   * @param   aDir
   *          The nsIFile for the directory to lock.
   */
  function lockDirectory(aDir) {
    var file = aDir.clone();
    file.append(kLockFileName);
    file.create(file.NORMAL_FILE_TYPE, 0o444);
    file.QueryInterface(AUS_Ci.nsILocalFileWin);
    file.fileAttributesWin |= file.WFA_READONLY;
    file.fileAttributesWin &= ~file.WFA_READWRITE;
    logTestInfo("testing the successful creation of the lock file");
    do_check_true(file.exists());
    do_check_false(file.isWritable());
  }
  /**
   * Helper function for unlocking a directory on Windows.
   *
   * @param   aDir
   *          The nsIFile for the directory to unlock.
   */
  function unlockDirectory(aDir) {
    var file = aDir.clone();
    file.append(kLockFileName);
    file.QueryInterface(AUS_Ci.nsILocalFileWin);
    file.fileAttributesWin |= file.WFA_READWRITE;
    file.fileAttributesWin &= ~file.WFA_READONLY;
    logTestInfo("removing and testing the successful removal of the lock file");
    file.remove(false);
    do_check_false(file.exists());
  }
}

/**
 * Helper function for updater tests for launching the updater binary to apply
 * a mar file.
 *
 * @param   aExpectedExitValue
 *          The expected exit value from the updater binary.
 * @param   aExpectedStatus
 *          The expected value of update.status when the test finishes.
 * @param   aCallback (optional)
 *          A callback function that will be called when this function finishes.
 *          If null no function will be called when this function finishes.
 *          If not specified the checkUpdateApplied function will be called when
 *          this function finishes.
 */
function runUpdate(aExpectedExitValue, aExpectedStatus, aCallback) {
  // Copy the updater binary to the updates directory.
  let binDir = gGREDirOrig.clone();
  let updater = binDir.clone();
  updater.append("updater.app");
  if (!updater.exists()) {
    updater = binDir.clone();
    updater.append(FILE_UPDATER_BIN);
    if (!updater.exists()) {
      do_throw("Unable to find updater binary!");
    }
  }

  let updatesDir = getUpdatesPatchDir();
  updater.copyToFollowingLinks(updatesDir, updater.leafName);
  let updateBin = updatesDir.clone();
  updateBin.append(updater.leafName);
  if (updateBin.leafName == "updater.app") {
    updateBin.append("Contents");
    updateBin.append("MacOS");
    updateBin.append("updater");
    if (!updateBin.exists()) {
      do_throw("Unable to find the updater executable!");
    }
  }

  let applyToDir = getApplyDirFile(null, true);
  let applyToDirPath = applyToDir.path;
  if (gStageUpdate || gSwitchApp) {
    applyToDirPath += "/" + DIR_UPDATED + "/";
  }

  if (IS_WIN) {
    // Convert to native path
    applyToDirPath = applyToDirPath.replace(/\//g, "\\");
  }

  let callbackApp = getApplyDirFile("a/b/" + gCallbackBinFile);
  callbackApp.permissions = PERMS_DIRECTORY;

  updateSettingsIni = getApplyDirFile(null, true);
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  let args = [updatesDir.path, applyToDirPath, 0];
  if (gStageUpdate) {
    args[2] = -1;
  } else {
    if (gSwitchApp) {
      args[2] = "0/replace";
    }
    args = args.concat([callbackApp.parent.path, callbackApp.path]);
    args = args.concat(gCallbackArgs);
  }
  logTestInfo("running the updater: " + updateBin.path + " " + args.join(" "));

  let env = AUS_Cc["@mozilla.org/process/environment;1"].
            getService(AUS_Ci.nsIEnvironment);
  if (gDisableReplaceFallback) {
    env.set("MOZ_NO_REPLACE_FALLBACK", "1");
  }

  let process = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  process.init(updateBin);
  process.run(true, args, args.length);

  if (gDisableReplaceFallback) {
    env.set("MOZ_NO_REPLACE_FALLBACK", "");
  }

  let status = readStatusFile();
  if (process.exitValue != aExpectedExitValue || status != aExpectedStatus) {
    if (process.exitValue != aExpectedExitValue) {
      logTestInfo("updater exited with unexpected value! Got: " +
                  process.exitValue + ", Expected: " +  aExpectedExitValue);
    }
    if (status != aExpectedStatus) {
      logTestInfo("update status is not the expected status! Got: " + status +
                  ", Expected: " +  aExpectedStatus);
    }
    let updateLog = getUpdatesPatchDir();
    updateLog.append(FILE_UPDATE_LOG);
    logTestInfo("contents of " + updateLog.path + ":\n" +
                readFileBytes(updateLog).replace(/\r\n/g, "\n"));
  }
  logTestInfo("testing updater binary process exitValue against expected " +
              "exit value");
  do_check_eq(process.exitValue, aExpectedExitValue);
  logTestInfo("testing update status against expected status");
  do_check_eq(status, aExpectedStatus);

  logTestInfo("testing updating directory doesn't exist");
  do_check_false(getUpdatingDir().exists());

  if (aCallback !== null) {
    if (typeof(aCallback) == typeof(Function)) {
      aCallback();
    } else {
      checkUpdateApplied();
    }
  }
}
/**
 * Helper function for updater tests to stage an update.
 */
function stageUpdate() {
  logTestInfo("start - staging update");
  Services.obs.addObserver(gUpdateStagedObserver, "update-staged", false);

  setEnvironment();
  // Stage the update.
  AUS_Cc["@mozilla.org/updates/update-processor;1"].
    createInstance(AUS_Ci.nsIUpdateProcessor).
    processUpdate(gUpdateManager.activeUpdate);
  resetEnvironment();

  logTestInfo("finish - staging update");
}

/**
 * Helper function to check whether the maintenance service updater tests should
 * run. See bug 711660 for more details.
 *
 * @param  aFirstTest
 *         Whether this is the first test within the test.
 * @return true if the test should run and false if it shouldn't.
 */
function shouldRunServiceTest(aFirstTest) {
  // In case the machine is running an old maintenance service or if it
  // is not installed, and permissions exist to install it.  Then install
  // the newer bin that we have.
  attemptServiceInstall();

  let binDir = getGREDir();
  let updaterBin = binDir.clone();
  updaterBin.append(FILE_UPDATER_BIN);
  if (!updaterBin.exists()) {
    do_throw("Unable to find updater binary!");
  }

  let updaterBinPath = updaterBin.path;
  if (/ /.test(updaterBinPath)) {
    updaterBinPath = '"' + updaterBinPath + '"';
  }

  const REG_PATH = "SOFTWARE\\Mozilla\\MaintenanceService\\" +
                   "3932ecacee736d366d6436db0f55bce4";

  let key = AUS_Cc["@mozilla.org/windows-registry-key;1"].
            createInstance(AUS_Ci.nsIWindowsRegKey);
  try {
    key.open(AUS_Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, REG_PATH,
             AUS_Ci.nsIWindowsRegKey.ACCESS_READ | key.WOW64_64);
  } catch (e) {
#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
    // The build system could sign the files and not have the test registry key
    // in which case we should fail the test by throwing so it can be fixed.
    if (isBinarySigned(updaterBinPath)) {
      do_throw("binary is signed but the test registry key does not exists!");
    }
#endif

    logTestInfo("this test can only run on the buildbot build system at this " +
                "time.");
    return false;
  }

  // Check to make sure the service is installed
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let args = ["wait-for-service-stop", "MozillaMaintenance", "10"];
  let process = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  process.init(helperBin);
  logTestInfo("checking if the service exists on this machine.");
  process.run(true, args, args.length);
  if (process.exitValue == 0xEE) {
    do_throw("test registry key exists but this test can only run on systems " +
             "with the maintenance service installed.");
  } else {
    logTestInfo("service exists, return value: " + process.exitValue);
  }

  // If this is the first test in the series, then there is no reason the
  // service should be anything but stopped, so be strict here and throw
  // an error.
  if (aFirstTest && process.exitValue != 0) {
    do_throw("First test, check for service stopped state returned error " +
             process.exitValue);
  }

#ifndef DISABLE_UPDATER_AUTHENTICODE_CHECK
  if (!isBinarySigned(updaterBinPath)) {
    logTestInfo("test registry key exists but this test can only run on " +
                "builds with signed binaries when " +
                "DISABLE_UPDATER_AUTHENTICODE_CHECK is not defined");
    do_throw("this test can only run on builds with signed binaries.");
  }
#endif
  return true;
}

/**
 * Helper function to check whether the a binary is signed.
 *
 * @param  aBinPath The path to the file to check if it is signed.
 * @return true if the file is signed and false if it isn't.
 */
function isBinarySigned(aBinPath) {
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let args = ["check-signature", aBinPath];
  let process = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  process.init(helperBin);
  process.run(true, args, args.length);
  if (process.exitValue != 0) {
    logTestInfo("binary is not signed. " + FILE_HELPER_BIN + " returned " +
                process.exitValue + " for file " + aBinPath);
    return false;
  }
  return true;
}

/**
 * Helper function for asynchronously setting up the application files required
 * to launch the application for the updater tests by either copying or creating
 * symlinks for the files. This is needed for Windows debug builds which can
 * lock a file that is being copied so that the tests can run in parallel. After
 * the files have been copied the setupAppFilesFinished function will be called.
 */
function setupAppFilesAsync() {
  gTimeoutRuns++;
  try {
    setupAppFiles();
  } catch (e) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while trying to setup application " +
               "files. Exception: " + e);
    }
    do_timeout(TEST_CHECK_TIMEOUT, setupAppFilesAsync);
    return;
  }

  setupAppFilesFinished();
}

/**
 * Helper function for setting up the application files required to launch the
 * application for the updater tests by either copying or creating symlinks for
 * the files.
 */
function setupAppFiles() {
  logTestInfo("start - copying or creating symlinks for application files " +
              "for the test");

  let srcDir = getCurrentProcessDir();
  let destDir = getApplyDirFile(null, true);
  if (!destDir.exists()) {
    try {
      destDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    } catch (e) {
      logTestInfo("unable to create directory, Path: " + destDir.path +
                  ", Exception: " + e);
      do_throw(e);
    }
  }

  // Required files for the application or the test that aren't listed in the
  // dependentlibs.list file.
  let fileRelPaths = [FILE_APP_BIN, FILE_UPDATER_BIN, FILE_UPDATE_SETTINGS_INI,
                       "application.ini", "dependentlibs.list"];

  // On Linux the updater.png must also be copied
  if (IS_UNIX && !IS_MACOSX) {
    fileRelPaths.push("icons/updater.png");
  }

  // Read the dependent libs file leafnames from the dependentlibs.list file
  // into the array.
  let deplibsFile = srcDir.clone();
  deplibsFile.append("dependentlibs.list");
  let istream = AUS_Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(AUS_Ci.nsIFileInputStream);
  istream.init(deplibsFile, 0x01, 0o444, 0);
  istream.QueryInterface(AUS_Ci.nsILineInputStream);

  let hasMore;
  let line = {};
  do {
    hasMore = istream.readLine(line);
    fileRelPaths.push(line.value);
  } while(hasMore);

  istream.close();

  fileRelPaths.forEach(function CMAF_FLN_FE(aFileRelPath) {
    copyFileToTestAppDir(aFileRelPath);
  });

  logTestInfo("finish - copying or creating symlinks for application files " +
              "for the test");
}

/**
 * Copies the specified files from the dist/bin directory into the test's
 * application directory.
 *
 * @param  aFileRelPath
 *         The relative path of the file to copy.
 */
function copyFileToTestAppDir(aFileRelPath) {
  let fileRelPath = aFileRelPath;
  let srcFile = gGREDirOrig.clone();
  let pathParts = fileRelPath.split("/");
  for (let i = 0; i < pathParts.length; i++) {
    if (pathParts[i]) {
      srcFile.append(pathParts[i]);
    }
  }

  if (IS_MACOSX && !srcFile.exists()) {
    logTestInfo("unable to copy file since it doesn't exist! Checking if " +
                 fileRelPath + ".app exists. Path: " +
                 srcFile.path);
    srcFile = gGREDirOrig.clone();
    for (let i = 0; i < pathParts.length; i++) {
      if (pathParts[i]) {
        srcFile.append(pathParts[i] + (pathParts.length - 1 == i ? ".app" : ""));
      }
    }
    fileRelPath = fileRelPath + ".app";
  }

  if (!srcFile.exists()) {
    do_throw("Unable to copy file since it doesn't exist! Path: " +
             srcFile.path);
  }

  // Symlink libraries. Note that the XUL library on Mac OS X doesn't have a
  // file extension and this will always be false on Windows.
  let shouldSymlink = (pathParts[pathParts.length - 1] == "XUL" ||
                       fileRelPath.substr(fileRelPath.length - 3) == ".so" ||
                       fileRelPath.substr(fileRelPath.length - 6) == ".dylib");
  let destFile = getApplyDirFile(DIR_BIN_REL_PATH + fileRelPath, true);
  if (!shouldSymlink) {
    if (!destFile.exists()) {
      try {
        srcFile.copyToFollowingLinks(destFile.parent, destFile.leafName);
      } catch (e) {
        // Just in case it is partially copied
        if (destFile.exists()) {
          try {
            destFile.remove(true);
          } catch (e) {
            logTestInfo("unable to remove file that failed to copy! Path: " +
                        destFile.path);
          }
        }
        do_throw("Unable to copy file! Path: " + srcFile.path +
                 ", Exception: " + e);
      }
    }
  } else {
    try {
      if (destFile.exists()) {
        destFile.remove(false);
      }
      let ln = AUS_Cc["@mozilla.org/file/local;1"].createInstance(AUS_Ci.nsILocalFile);
      ln.initWithPath("/bin/ln");
      let process = AUS_Cc["@mozilla.org/process/util;1"].createInstance(AUS_Ci.nsIProcess);
      process.init(ln);
      let args = ["-s", srcFile.path, destFile.path];
      process.run(true, args, args.length);
      logTestInfo("verifying symlink. Path: " + destFile.path);
      do_check_true(destFile.isSymlink());
    } catch (e) {
      do_throw("Unable to create symlink for file! Path: " + srcFile.path +
               ", Exception: " + e);
    }
  }
}

/**
 * Attempts to upgrade the maintenance service if permissions are allowed.
 * This is useful for XP where we have permission to upgrade in case an
 * older service installer exists.  Also if the user manually installed into
 * a unprivileged location.
 */
function attemptServiceInstall() {
  var version = AUS_Cc["@mozilla.org/system-info;1"]
                .getService(AUS_Ci.nsIPropertyBag2)
                .getProperty("version");
  var isVistaOrHigher = (parseFloat(version) >= 6.0);
  if (isVistaOrHigher) {
    return;
  }

  let binDir = getGREDir();
  let installerFile = binDir.clone();
  installerFile.append(FILE_MAINTENANCE_SERVICE_INSTALLER_BIN);
  if (!installerFile.exists()) {
    do_throw(FILE_MAINTENANCE_SERVICE_INSTALLER_BIN + " not found.");
  }
  let installerProcess = AUS_Cc["@mozilla.org/process/util;1"].
                         createInstance(AUS_Ci.nsIProcess);
  installerProcess.init(installerFile);
  logTestInfo("starting installer process...");
  installerProcess.run(true, [], 0);
}

/**
 * Helper function for updater tests for launching the updater using the
 * maintenance service to apply a mar file.
 *
 * @param aInitialStatus
 *        The initial value of update.status.
 * @param aExpectedStatus
 *        The expected value of update.status when the test finishes.
 * @param aCheckSvcLog
 *        Whether the service log should be checked (optional).
 */
function runUpdateUsingService(aInitialStatus, aExpectedStatus, aCheckSvcLog) {
  // Check the service logs for a successful update
  function checkServiceLogs(aOriginalContents) {
    let contents = readServiceLogFile();
    logTestInfo("the contents of maintenanceservice.log:\n" + contents + "\n");
    do_check_neq(contents, aOriginalContents);
    do_check_neq(contents.indexOf(LOG_SVC_SUCCESSFUL_LAUNCH), -1);
  }
  function readServiceLogFile() {
    let file = AUS_Cc["@mozilla.org/file/directory_service;1"].
               getService(AUS_Ci.nsIProperties).
               get("CmAppData", AUS_Ci.nsIFile);
    file.append("Mozilla");
    file.append("logs");
    file.append("maintenanceservice.log");
    return readFile(file);
  }
  function waitServiceApps() {
    // maintenanceservice_installer.exe is started async during updates.
    waitForApplicationStop("maintenanceservice_installer.exe");
    // maintenanceservice_tmp.exe is started async from the service installer.
    waitForApplicationStop("maintenanceservice_tmp.exe");
    // In case the SCM thinks the service is stopped, but process still exists.
    waitForApplicationStop("maintenanceservice.exe");
  }
  function waitForServiceStop(aFailTest) {
    waitServiceApps();
    logTestInfo("waiting for service to stop if necessary...");
    // Use the helper bin to ensure the service is stopped. If not
    // stopped then wait for the service to be stopped (at most 120 seconds)
    let helperBin = getTestDirFile(FILE_HELPER_BIN);
    let helperBinArgs = ["wait-for-service-stop",
                         "MozillaMaintenance",
                         "120"];
    let helperBinProcess = AUS_Cc["@mozilla.org/process/util;1"].
                           createInstance(AUS_Ci.nsIProcess);
    helperBinProcess.init(helperBin);
    logTestInfo("stopping service...");
    helperBinProcess.run(true, helperBinArgs, helperBinArgs.length);
    if (helperBinProcess.exitValue == 0xEE) {
      do_throw("The service does not exist on this machine.  Return value: " +
               helperBinProcess.exitValue);
    } else if (helperBinProcess.exitValue != 0) {
      if (aFailTest) {
        do_throw("maintenance service did not stop, last state: " +
                 helperBinProcess.exitValue + ". Forcing test failure.");
      } else {
        logTestInfo("maintenance service did not stop, last state: " +
                    helperBinProcess.exitValue + ".  May cause failures.");
      }
    } else {
      logTestInfo("service stopped.");
    }
    waitServiceApps();
  }
  function waitForApplicationStop(aApplication) {
    logTestInfo("waiting for " + aApplication + " to stop if " +
                "necessary...");
    // Use the helper bin to ensure the application is stopped.
    // If not, then wait for it to be stopped (at most 120 seconds)
    let helperBin = getTestDirFile(FILE_HELPER_BIN);
    let helperBinArgs = ["wait-for-application-exit",
                         aApplication,
                         "120"];
    let helperBinProcess = AUS_Cc["@mozilla.org/process/util;1"].
                           createInstance(AUS_Ci.nsIProcess);
    helperBinProcess.init(helperBin);
    helperBinProcess.run(true, helperBinArgs, helperBinArgs.length);
    if (helperBinProcess.exitValue != 0) {
      do_throw(aApplication + " did not stop, last state: " +
               helperBinProcess.exitValue + ". Forcing test failure.");
    }
  }

  // Make sure the service from the previous test is already stopped.
  waitForServiceStop(true);

  // Prevent the cleanup function from begin run more than once
  if (gRegisteredServiceCleanup === undefined) {
    gRegisteredServiceCleanup = true;

    do_register_cleanup(function RUUS_cleanup() {
      resetEnvironment();

      // This will delete the app arguments log file if it exists.
      try {
        getAppArgsLogPath();
      } catch (e) {
        logTestInfo("unable to remove file during cleanup. Exception: " + e);
      }
    });
  }

  let svcOriginalLog;
  // Default to checking the service log if the parameter is not specified.
  if (aCheckSvcLog === undefined || aCheckSvcLog) {
    svcOriginalLog = readServiceLogFile();
  }

  let appArgsLogPath = getAppArgsLogPath();
  gServiceLaunchedCallbackLog = appArgsLogPath.replace(/^"|"$/g, "");

  let updatesDir = getUpdatesPatchDir();
  let file = updatesDir.clone();
  writeStatusFile(aInitialStatus);

  // sanity check
  do_check_eq(readStatusState(), aInitialStatus);

  writeVersionFile(DEFAULT_UPDATE_VERSION);

  gServiceLaunchedCallbackArgs = [
    "-no-remote",
    "-process-updates",
    "-dump-args",
    appArgsLogPath
  ];

  if (gSwitchApp) {
    // We want to set the env vars again
    gShouldResetEnv = undefined;
  }

  setEnvironment();

  // There is a security check done by the service to make sure the updater
  // we are executing is the same as the one in the apply-to dir.
  // To make sure they match from tests we copy updater.exe to the apply-to dir.
  copyFileToTestAppDir(FILE_UPDATER_BIN);

  // The service will execute maintenanceservice_installer.exe and
  // will copy maintenanceservice.exe out of the same directory from
  // the installation directory.  So we need to make sure both of those
  // bins always exist in the installation directory.
  copyFileToTestAppDir(FILE_MAINTENANCE_SERVICE_BIN);
  copyFileToTestAppDir(FILE_MAINTENANCE_SERVICE_INSTALLER_BIN);

  let updateSettingsIni = getApplyDirFile(null, true);
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);

  let launchBin = getLaunchBin();
  let args = getProcessArgs(["-dump-args", appArgsLogPath]);

  let process = AUS_Cc["@mozilla.org/process/util;1"].
                   createInstance(AUS_Ci.nsIProcess);
  process.init(launchBin);
  logTestInfo("launching " + launchBin.path + " " + args.join(" "));
  // Firefox does not wait for the service command to finish, but
  // we still launch the process sync to avoid intermittent failures with
  // the log file not being written out yet.
  // We will rely on watching the update.status file and waiting for the service
  // to stop to know the service command is done.
  process.run(true, args, args.length);

  resetEnvironment();

  function timerCallback(aTimer) {
    // Wait for the expected status
    let status = readStatusState();
    // status will probably always be equal to STATE_APPLYING but there is a
    // race condition where it would be possible on slower machines where status
    // could be equal to STATE_PENDING_SVC.
    if (status == STATE_APPLYING ||
        status == STATE_PENDING_SVC) {
      logTestInfo("still waiting to see the " + aExpectedStatus +
                  " status, got " + status + " for now...");
      return;
    }

    // Make sure all of the logs are written out.
    waitForServiceStop(false);

    aTimer.cancel();
    aTimer = null;

    if (status != aExpectedStatus) {
      logTestInfo("update status is not the expected status! Got: " + status +
                  ", Expected: " +  aExpectedStatus);
      logTestInfo("update.status contents: " + readStatusFile());
      let updateLog = getUpdatesPatchDir();
      updateLog.append(FILE_UPDATE_LOG);
      logTestInfo("contents of " + updateLog.path + ":\n" +
                  readFileBytes(updateLog).replace(/\r\n/g, "\n"));
    }
    logTestInfo("testing update status against expected status");
    do_check_eq(status, aExpectedStatus);

    if (aCheckSvcLog) {
      checkServiceLogs(svcOriginalLog);
    }

    logTestInfo("testing updating directory doesn't exist");
    do_check_false(getUpdatingDir().exists());

    checkUpdateFinished();
  }

  let timer = AUS_Cc["@mozilla.org/timer;1"].createInstance(AUS_Ci.nsITimer);
  timer.initWithCallback(timerCallback, 1000, timer.TYPE_REPEATING_SLACK);
}

/**
 * Gets the platform specific shell binary that is launched using nsIProcess and
 * in turn launches a binary used for the test (e.g. application, updater,
 * etc.). A shell is used so debug console output can be redirected to a file so
 * it doesn't end up in the test log.
 *
 * @return  nsIFile for the shell binary to launch using nsIProcess.
 * @throws  if the shell binary doesn't exist.
 */
function getLaunchBin() {
  let launchBin;
  if (IS_WIN) {
    launchBin = Services.dirsvc.get("WinD", AUS_Ci.nsIFile);
    launchBin.append("System32");
    launchBin.append("cmd.exe");
  } else {
    launchBin = AUS_Cc["@mozilla.org/file/local;1"].
                createInstance(AUS_Ci.nsILocalFile);
    launchBin.initWithPath("/bin/sh");
  }

  if (!launchBin.exists())
    do_throw(launchBin.path + " must exist to run this test!");

  return launchBin;
}

/**
 * Helper function that waits until the helper has completed its operations and
 * is in a sleep state before performing an update by calling doUpdate.
 */
function waitForHelperSleep() {
  gTimeoutRuns++;
  // Give the lock file process time to lock the file before updating otherwise
  // this test can fail intermittently on Windows debug builds.
  let output = getApplyDirFile("a/b/output", true);
  if (readFile(output) != "sleeping\n") {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the helper to " +
               "finish its operation. Path: " + output.path);
    }
    do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
    return;
  }
  try {
    output.remove(false);
  }
  catch (e) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the helper " +
               "message file to no longer be in use. Path: " + output.path);
    }
    logTestInfo("failed to remove file. Path: " + output.path);
    do_timeout(TEST_HELPER_TIMEOUT, waitForHelperSleep);
    return;
  }
  doUpdate();
}

/**
 * Helper function that waits until the helper has finished its operations
 * before calling waitForHelperFinishFileUnlock to verify that the helper's
 * input and output directories are no longer in use.
 */
function waitForHelperFinished() {
  // Give the lock file process time to lock the file before updating otherwise
  // this test can fail intermittently on Windows debug builds.
  let output = getApplyDirFile("a/b/output", true);
  if (readFile(output) != "finished\n") {
    do_timeout(TEST_HELPER_TIMEOUT, waitForHelperFinished);
    return;
  }
  // Give the lock file process time to unlock the file before deleting the
  // input and output files.
  waitForHelperFinishFileUnlock();
}

/**
 * Helper function that waits until the helper's input and output files are no
 * longer in use before calling checkUpdate.
 */
function waitForHelperFinishFileUnlock() {
  try {
    let output = getApplyDirFile("a/b/output", true);
    if (output.exists()) {
      output.remove(false);
    }
    let input = getApplyDirFile("a/b/input", true);
    if (input.exists()) {
      input.remove(false);
    }
  } catch (e) {
    // Give the lock file process time to unlock the file before deleting the
    // input and output files.
    do_timeout(TEST_HELPER_TIMEOUT, waitForHelperFinishFileUnlock);
    return;
  }
  checkUpdate();
}

/**
 * Helper function to tell the helper to finish and exit its sleep state.
 */
function setupHelperFinish() {
  let input = getApplyDirFile("a/b/input", true);
  writeFile(input, "finish\n");
  waitForHelperFinished();
}

/**
 * Helper function for updater binary tests that creates the files and
 * directories used by the test.
 *
 * @param   aMarFile
 *          The mar file for the update test.
 */
function setupUpdaterTest(aMarFile) {
  let updatesPatchDir = getUpdatesPatchDir();
  if (!updatesPatchDir.exists()) {
    updatesPatchDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  // Copy the mar that will be applied
  let mar = getTestDirFile(aMarFile);
  mar.copyToFollowingLinks(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  let applyToDir = getApplyDirFile(null, true);
  TEST_FILES.forEach(function SUT_TF_FE(aTestFile) {
    if (aTestFile.originalFile || aTestFile.originalContents) {
      let testDir = getApplyDirFile(aTestFile.relPathDir, true);
      if (!testDir.exists())
        testDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);

      let testFile;
      if (aTestFile.originalFile) {
        testFile = getTestDirFile(aTestFile.originalFile);
        testFile.copyToFollowingLinks(testDir, aTestFile.fileName);
        testFile = getApplyDirFile(aTestFile.relPathDir + aTestFile.fileName);
      } else {
        testFile = getApplyDirFile(aTestFile.relPathDir + aTestFile.fileName,
                                   true);
        writeFile(testFile, aTestFile.originalContents);
      }

      // Skip these tests on Windows and OS/2 since their
      // implementaions of chmod doesn't really set permissions.
      if (!IS_WIN && !IS_OS2 && aTestFile.originalPerms) {
        testFile.permissions = aTestFile.originalPerms;
        // Store the actual permissions on the file for reference later after
        // setting the permissions.
        if (!aTestFile.comparePerms) {
          aTestFile.comparePerms = testFile.permissions;
        }
      }
    }
  });

  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let afterApplyBinDir = getApplyDirFile("a/b/", true);
  helperBin.copyToFollowingLinks(afterApplyBinDir, gCallbackBinFile);

  // Add the test directory that will be updated for a successful update or left
  // in the initial state for a failed update.
  var testDirs = TEST_DIRS.concat(ADDITIONAL_TEST_DIRS);
  testDirs.forEach(function SUT_TD_FE(aTestDir) {
    let testDir = getApplyDirFile(aTestDir.relPathDir, true);
    if (!testDir.exists()) {
      testDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    }

    if (aTestDir.files) {
      aTestDir.files.forEach(function SUT_TD_F_FE(aTestFile) {
        let testFile = getApplyDirFile(aTestDir.relPathDir + aTestFile, true);
        if (!testFile.exists()) {
          testFile.create(AUS_Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
        }
      });
    }

    if (aTestDir.subDirs) {
      aTestDir.subDirs.forEach(function SUT_TD_SD_FE(aSubDir) {
        let testSubDir = getApplyDirFile(aTestDir.relPathDir + aSubDir, true);
        if (!testSubDir.exists()) {
          testSubDir.create(AUS_Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
        }

        if (aTestDir.subDirFiles) {
          aTestDir.subDirFiles.forEach(function SUT_TD_SDF_FE(aTestFile) {
            let testFile = getApplyDirFile(aTestDir.relPathDir + aSubDir + aTestFile, true);
            if (!testFile.exists()) {
              testFile.create(AUS_Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
            }
          });
        }
      });
    }
  });
}

/**
 * Helper function for updater binary tests for verifying the contents of the
 * update log after a successful update.
 *
 * @param   aCompareLogFile
 *          The log file to compare the update log with.
 */
function checkUpdateLogContents(aCompareLogFile) {
  let updateLog = getUpdatesPatchDir();
  updateLog.append(FILE_UPDATE_LOG);
  let updateLogContents = readFileBytes(updateLog);

  if (gStageUpdate) {
    // Skip the staged update messages
    updateLogContents = updateLogContents.replace(/Performing a staged update/, "");
  } else if (gSwitchApp) {
    // Skip the switch app request messages
    updateLogContents = updateLogContents.replace(/Performing a staged update/, "");
    updateLogContents = updateLogContents.replace(/Performing a replace request/, "");
  }
  // Skip the source/destination lines since they contain absolute paths.
  updateLogContents = updateLogContents.replace(/SOURCE DIRECTORY.*/g, "");
  updateLogContents = updateLogContents.replace(/DESTINATION DIRECTORY.*/g, "");
  // Skip lines that log failed attempts to open the callback executable.
  updateLogContents = updateLogContents.replace(/NS_main: callback app file .*/g, "");
  if (gSwitchApp) {
    // Remove the lines which contain absolute paths
    updateLogContents = updateLogContents.replace(/^Begin moving.*$/mg, "");
#ifdef XP_MACOSX
    // Remove the entire section about moving the precomplete file as it contains
    // absolute paths.
    updateLogContents = updateLogContents.replace(/\n/g, "%%%EOL%%%");
    updateLogContents = updateLogContents.replace(/Moving the precomplete file.*Finished moving the precomplete file/, "");
    updateLogContents = updateLogContents.replace(/%%%EOL%%%/g, "\n");
#endif
  }
  updateLogContents = updateLogContents.replace(/\r/g, "");
  // Replace error codes since they are different on each platform.
  updateLogContents = updateLogContents.replace(/, err:.*\n/g, "\n");
  // Replace to make the log parsing happy.
  updateLogContents = updateLogContents.replace(/non-fatal error /g, "");
  // The FindFile results when enumerating the filesystem on Windows is not
  // determistic so the results matching the following need to be ignored.
  updateLogContents = updateLogContents.replace(/.* a\/b\/7\/7text.*\n/g, "");
  // Remove consecutive newlines
  updateLogContents = updateLogContents.replace(/\n+/g, "\n");
  // Remove leading and trailing newlines
  updateLogContents = updateLogContents.replace(/^\n|\n$/g, "");
  // The update log when running the service tests sometimes starts with data
  // from the previous launch of the updater.
  updateLogContents = updateLogContents.replace(/^calling QuitProgressUI\n[^\n]*\nUPDATE TYPE/g, "UPDATE TYPE");

  let compareLog = getTestDirFile(aCompareLogFile);
  let compareLogContents = readFileBytes(compareLog);
  // Remove leading and trailing newlines
  compareLogContents = compareLogContents.replace(/^\n|\n$/g, "");

  // Don't write the contents of the file to the log to reduce log spam
  // unless there is a failure.
  if (compareLogContents == updateLogContents) {
    logTestInfo("log contents are correct");
    do_check_true(true);
  } else {
    logTestInfo("log contents are not correct");
    do_check_eq(compareLogContents, updateLogContents);
  }
}

/**
 * Helper function to check if the update log contains a string.
 *
 * @param   aCheckString
 *          The string to check if the update log contains.
 */
function checkUpdateLogContains(aCheckString) {
  let updateLog = getUpdatesPatchDir();
  updateLog.append(FILE_UPDATE_LOG);
  let updateLogContents = readFileBytes(updateLog);
  if (updateLogContents.indexOf(aCheckString) != -1) {
    logTestInfo("log file does contain: " + aCheckString);
    do_check_true(true);
  } else {
    logTestInfo("log file does not contain: " + aCheckString);
    logTestInfo("log file contents:\n" + updateLogContents);
    do_check_true(false);
  }
}

/**
 * Helper function for updater binary tests for verifying the state of files and
 * directories after a successful update.
 */
function checkFilesAfterUpdateSuccess() {
  logTestInfo("testing contents of files after a successful update");
  TEST_FILES.forEach(function CFAUS_TF_FE(aTestFile) {
    let testFile = getTargetDirFile(aTestFile.relPathDir + aTestFile.fileName,
                                    true);
    logTestInfo("testing file: " + testFile.path);
    if (aTestFile.compareFile || aTestFile.compareContents) {
      do_check_true(testFile.exists());

      // Skip these tests on Windows and OS/2 since their
      // implementaions of chmod doesn't really set permissions.
      if (!IS_WIN && !IS_OS2 && aTestFile.comparePerms) {
        // Check if the permssions as set in the complete mar file are correct.
        let logPerms = "testing file permissions - ";
        if (aTestFile.originalPerms) {
          logPerms += "original permissions: " +
                      aTestFile.originalPerms.toString(8) + ", ";
        }
        logPerms += "compare permissions : " +
                    aTestFile.comparePerms.toString(8) + ", ";
        logPerms += "updated permissions : " + testFile.permissions.toString(8);
        logTestInfo(logPerms);
        do_check_eq(testFile.permissions & 0xfff, aTestFile.comparePerms & 0xfff);
      }

      let fileContents1 = readFileBytes(testFile);
      let fileContents2 = aTestFile.compareFile ?
                          readFileBytes(getTestDirFile(aTestFile.compareFile)) :
                          aTestFile.compareContents;
      // Don't write the contents of the file to the log to reduce log spam
      // unless there is a failure.
      if (fileContents1 == fileContents2) {
        logTestInfo("file contents are correct");
        do_check_true(true);
      } else {
        logTestInfo("file contents are not correct");
        do_check_eq(fileContents1, fileContents2);
      }
    } else {
      do_check_false(testFile.exists());
    }
  });

  logTestInfo("testing operations specified in removed-files were performed " +
              "after a successful update");
  var testDirs = TEST_DIRS.concat(ADDITIONAL_TEST_DIRS);
  testDirs.forEach(function CFAUS_TD_FE(aTestDir) {
    let testDir = getTargetDirFile(aTestDir.relPathDir, true);
    logTestInfo("testing directory: " + testDir.path);
    if (aTestDir.dirRemoved) {
      do_check_false(testDir.exists());
    } else {
      do_check_true(testDir.exists());

      if (aTestDir.files) {
        aTestDir.files.forEach(function CFAUS_TD_F_FE(aTestFile) {
          let testFile = getTargetDirFile(aTestDir.relPathDir + aTestFile, true);
          logTestInfo("testing directory file: " + testFile.path);
          if (aTestDir.filesRemoved) {
            do_check_false(testFile.exists());
          } else {
            do_check_true(testFile.exists());
          }
        });
      }

      if (aTestDir.subDirs) {
        aTestDir.subDirs.forEach(function CFAUS_TD_SD_FE(aSubDir) {
          let testSubDir = getTargetDirFile(aTestDir.relPathDir + aSubDir, true);
          logTestInfo("testing sub-directory: " + testSubDir.path);
          do_check_true(testSubDir.exists());
          if (aTestDir.subDirFiles) {
            aTestDir.subDirFiles.forEach(function CFAUS_TD_SDF_FE(aTestFile) {
              let testFile = getTargetDirFile(aTestDir.relPathDir + aSubDir + aTestFile, true);
              logTestInfo("testing sub-directory file: " + testFile.path);
              do_check_true(testFile.exists());
            });
          }
        });
      }
    }
  });

  checkFilesAfterUpdateCommon();
}

/**
 * Helper function for updater binary tests for verifying the state of files and
 * directories after a failed update.
 *
 * @param aGetDirectory: the function used to get the files in the target directory.
 * Pass getApplyDirFile if you want to test the case of a failed switch request.
 */
function checkFilesAfterUpdateFailure(aGetDirectory) {
  let getdir = aGetDirectory || getTargetDirFile;
  logTestInfo("testing contents of files after a failed update");
  TEST_FILES.forEach(function CFAUF_TF_FE(aTestFile) {
    let testFile = getdir(aTestFile.relPathDir + aTestFile.fileName, true);
    logTestInfo("testing file: " + testFile.path);
    if (aTestFile.compareFile || aTestFile.compareContents) {
      do_check_true(testFile.exists());

      // Skip these tests on Windows and OS/2 since their
      // implementaions of chmod doesn't really set permissions.
      if (!IS_WIN && !IS_OS2 && aTestFile.comparePerms) {
        // Check the original permssions are retained on the file.
        let logPerms = "testing file permissions - ";
        if (aTestFile.originalPerms) {
          logPerms += "original permissions: " +
                      aTestFile.originalPerms.toString(8) + ", ";
        }
        logPerms += "compare permissions : " +
                    aTestFile.comparePerms.toString(8) + ", ";
        logPerms += "updated permissions : " + testFile.permissions.toString(8);
        logTestInfo(logPerms);
        do_check_eq(testFile.permissions & 0xfff, aTestFile.comparePerms & 0xfff);
      }

      let fileContents1 = readFileBytes(testFile);
      let fileContents2 = aTestFile.compareFile ?
                          readFileBytes(getTestDirFile(aTestFile.compareFile)) :
                          aTestFile.compareContents;
      // Don't write the contents of the file to the log to reduce log spam
      // unless there is a failure.
      if (fileContents1 == fileContents2) {
        logTestInfo("file contents are correct");
        do_check_true(true);
      } else {
        logTestInfo("file contents are not correct");
        do_check_eq(fileContents1, fileContents2);
      }
    } else {
      do_check_false(testFile.exists());
    }
  });

  logTestInfo("testing operations specified in removed-files were not " +
              "performed after a failed update");
  TEST_DIRS.forEach(function CFAUF_TD_FE(aTestDir) {
    let testDir = getdir(aTestDir.relPathDir, true);
    logTestInfo("testing directory file: " + testDir.path);
    do_check_true(testDir.exists());

    if (aTestDir.files) {
      aTestDir.files.forEach(function CFAUS_TD_F_FE(aTestFile) {
        let testFile = getdir(aTestDir.relPathDir + aTestFile, true);
        logTestInfo("testing directory file: " + testFile.path);
        do_check_true(testFile.exists());
      });
    }

    if (aTestDir.subDirs) {
      aTestDir.subDirs.forEach(function CFAUS_TD_SD_FE(aSubDir) {
        let testSubDir = getdir(aTestDir.relPathDir + aSubDir, true);
        logTestInfo("testing sub-directory: " + testSubDir.path);
        do_check_true(testSubDir.exists());
        if (aTestDir.subDirFiles) {
          aTestDir.subDirFiles.forEach(function CFAUS_TD_SDF_FE(aTestFile) {
            let testFile = getdir(aTestDir.relPathDir + aSubDir + aTestFile,
                                  true);
            logTestInfo("testing sub-directory file: " + testFile.path);
            do_check_true(testFile.exists());
          });
        }
      });
    }
  });

  checkFilesAfterUpdateCommon();
}

/**
 * Helper function for updater binary tests for verifying patch files and
 * moz-backup files aren't left behind after a successful or failed update.
 */
function checkFilesAfterUpdateCommon() {
  logTestInfo("testing patch files should not be left behind");
  let updatesDir = getUpdatesPatchDir();
  let entries = updatesDir.QueryInterface(AUS_Ci.nsIFile).directoryEntries;
  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(AUS_Ci.nsIFile);
    do_check_neq(getFileExtension(entry), "patch");
  }

  logTestInfo("testing backup files should not be left behind");
  let applyToDir = getTargetDirFile(null, true);
  checkFilesInDirRecursive(applyToDir, checkForBackupFiles);
}

/**
 * Helper function for updater binary tests for verifying the contents of the
 * updater callback application log which should contain the arguments passed to
 * the callback application.
 */
function checkCallbackAppLog() {
  let appLaunchLog = getApplyDirFile("a/b/" + gCallbackArgs[1], true);
  if (!appLaunchLog.exists()) {
    do_timeout(TEST_HELPER_TIMEOUT, checkCallbackAppLog);
    return;
  }

  let expectedLogContents = gCallbackArgs.join("\n") + "\n";
  let logContents = readFile(appLaunchLog);
  // It is possible for the log file contents check to occur before the log file
  // contents are completely written so wait until the contents are the expected
  // value. If the contents are never the expected value then the test will
  // fail by timing out.
  if (logContents != expectedLogContents) {
    do_timeout(TEST_HELPER_TIMEOUT, checkCallbackAppLog);
    return;
  }

  if (logContents == expectedLogContents) {
    logTestInfo("callback log file contents are correct");
    do_check_true(true);
  } else {
    logTestInfo("callback log file contents are not correct");
    do_check_eq(logContents, expectedLogContents);
  }

  waitForFilesInUse();
}

/**
 * Helper function for updater service tests for verifying the contents of the
 * updater callback application log which should contain the arguments passed to
 * the callback application.
 */
function checkCallbackServiceLog() {
  do_check_neq(gServiceLaunchedCallbackLog, null);

  let expectedLogContents = gServiceLaunchedCallbackArgs.join("\n") + "\n";
  let logFile = AUS_Cc["@mozilla.org/file/local;1"].createInstance(AUS_Ci.nsILocalFile);
  logFile.initWithPath(gServiceLaunchedCallbackLog);
  let logContents = readFile(logFile);
  // It is possible for the log file contents check to occur before the log file
  // contents are completely written so wait until the contents are the expected
  // value. If the contents are never the expected value then the test will
  // fail by timing out.
  if (logContents != expectedLogContents) {
    logTestInfo("callback service log not expected value, waiting longer");
    do_timeout(TEST_HELPER_TIMEOUT, checkCallbackServiceLog);
    return;
  }

  logTestInfo("testing that the callback application successfully launched " +
              "and the expected command line arguments were passed to it");
  do_check_eq(logContents, expectedLogContents);

  waitForFilesInUse();
}

// Waits until files that are in use that break tests are no longer in use and
// then calls do_test_finished.
function waitForFilesInUse() {
  if (IS_WIN) {
    let appBin = getApplyDirFile(FILE_APP_BIN, true);
    let maintSvcInstaller = getApplyDirFile(FILE_MAINTENANCE_SERVICE_INSTALLER_BIN, true);
    let helper = getApplyDirFile("uninstall/helper.exe", true);
    let updater = getUpdatesPatchDir();
    updater.append(FILE_UPDATER_BIN);

    let files = [appBin, updater, maintSvcInstaller, helper];

    for (var i = 0; i < files.length; ++i) {
      let file = files[i];
      let fileBak = file.parent.clone();
      if (file.exists()) {
        fileBak.append(file.leafName + ".bak");
        try {
          if (fileBak.exists()) {
            fileBak.remove(false);
          }
          file.copyTo(fileBak.parent, fileBak.leafName);
          file.remove(false);
          fileBak.moveTo(file.parent, file.leafName);
          logTestInfo("file is not in use. Path: " + file.path);
        } catch (e) {
          logTestInfo("file in use, will try again after " + TEST_CHECK_TIMEOUT +
                      " ms, Path: " + file.path + ", Exception: " + e);
          try {
            if (fileBak.exists()) {
              fileBak.remove(false);
            }
          } catch (e) {
            logTestInfo("unable to remove file, this should never happen! " +
                        "Path: " + fileBak.path + ", Exception: " + e);
          }
          do_timeout(TEST_CHECK_TIMEOUT, waitForFilesInUse);
          return;
        }
      }
    }
  }

  logTestInfo("calling doTestFinish");
  doTestFinish();
}

/**
 * Helper function for updater binary tests for verifying there are no update
 * backup files left behind after an update.
 *
 * @param   aFile
 *          An nsIFile to check if it has moz-backup for its extension.
 */
function checkForBackupFiles(aFile) {
  do_check_neq(getFileExtension(aFile), "moz-backup");
}

/**
 * Helper function for updater binary tests for recursively enumerating a
 * directory and calling a callback function with the file as a parameter for
 * each file found.
 *
 * @param   aDir
 *          A nsIFile for the directory to be deleted
 * @param   aCallback
 *          A callback function that will be called with the file as a
 *          parameter for each file found.
 */
function checkFilesInDirRecursive(aDir, aCallback) {
  if (!aDir.exists())
    do_throw("Directory must exist!");

  let dirEntries = aDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    let entry = dirEntries.getNext().QueryInterface(AUS_Ci.nsIFile);

    if (entry.isDirectory()) {
      checkFilesInDirRecursive(entry, aCallback);
    } else {
      aCallback(entry);
    }
  }
}

/**
 * Sets up the bare bones XMLHttpRequest implementation below.
 *
 * @param   aCallback
 *          The callback function that will call the nsIDomEventListener's
 *          handleEvent method.
 *
 *          Example of the callback function
 *
 *            function callHandleEvent() {
 *              gXHR.status = gExpectedStatus;
 *              var e = { target: gXHR };
 *              gXHR.onload.handleEvent(e);
 *            }
 */
function overrideXHR(aCallback) {
  gXHRCallback = aCallback;
  gXHR = new xhr();
  var registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(gXHR.classID, gXHR.classDescription,
                            gXHR.contractID, gXHR);
}


/**
 * Bare bones XMLHttpRequest implementation for testing onprogress, onerror,
 * and onload nsIDomEventListener handleEvent.
 */
function makeHandler(aVal) {
  if (typeof aVal == "function")
    return { handleEvent: aVal };
  return aVal;
}
function xhr() {
}
xhr.prototype = {
  overrideMimeType: function(aMimetype) { },
  setRequestHeader: function(aHeader, aValue) { },
  status: null,
  channel: { set notificationCallbacks(aVal) { } },
  _url: null,
  _method: null,
  open: function(aMethod, aUrl) {
    gXHR.channel.originalURI = Services.io.newURI(aUrl, null, null);
    gXHR._method = aMethod; gXHR._url = aUrl;
  },
  responseXML: null,
  responseText: null,
  send: function(aBody) {
    do_execute_soon(gXHRCallback); // Use a timeout so the XHR completes
  },
  _onprogress: null,
  set onprogress(aValue) { gXHR._onprogress = makeHandler(aValue); },
  get onprogress() { return gXHR._onprogress; },
  _onerror: null,
  set onerror(aValue) { gXHR._onerror = makeHandler(aValue); },
  get onerror() { return gXHR._onerror; },
  _onload: null,
  set onload(aValue) { gXHR._onload = makeHandler(aValue); },
  get onload() { return gXHR._onload; },
  addEventListener: function(aEvent, aValue, aCapturing) {
    eval("gXHR._on" + aEvent + " = aValue");
  },
  flags: AUS_Ci.nsIClassInfo.SINGLETON,
  implementationLanguage: AUS_Ci.nsIProgrammingLanguage.JAVASCRIPT,
  getHelperForLanguage: function(aLanguage) null,
  getInterfaces: function(aCount) {
    var interfaces = [AUS_Ci.nsISupports];
    aCount.value = interfaces.length;
    return interfaces;
  },
  classDescription: "XMLHttpRequest",
  contractID: "@mozilla.org/xmlextras/xmlhttprequest;1",
  classID: Components.ID("{c9b37f43-4278-4304-a5e0-600991ab08cb}"),
  createInstance: function(aOuter, aIID) {
    if (aOuter == null)
      return gXHR.QueryInterface(aIID);
    throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(AUS_Ci.nsIClassInfo) ||
        aIID.equals(AUS_Ci.nsISupports))
      return gXHR;
    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  },
  get wrappedJSObject() { return this; }
};

/**
 * Helper function to override the update prompt component to verify whether it
 * is called or not.
 *
 * @param   aCallback
 *          The callback to call if the update prompt component is called.
 */
function overrideUpdatePrompt(aCallback) {
  var registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  gUpdatePrompt = new UpdatePrompt();
  gUpdatePromptCallback = aCallback;
  registrar.registerFactory(gUpdatePrompt.classID, gUpdatePrompt.classDescription,
                            gUpdatePrompt.contractID, gUpdatePrompt);
}

function UpdatePrompt() {
  var fns = ["checkForUpdates", "showUpdateAvailable", "showUpdateDownloaded",
             "showUpdateError", "showUpdateHistory", "showUpdateInstalled"];

  fns.forEach(function(aPromptFn) {
    UpdatePrompt.prototype[aPromptFn] = function() {
      if (!gUpdatePromptCallback) {
        return;
      }

      var callback = gUpdatePromptCallback[aPromptFn];
      if (!callback) {
        return;
      }

      callback.apply(gUpdatePromptCallback,
                     Array.prototype.slice.call(arguments));
    }
  });
}

UpdatePrompt.prototype = {
  flags: AUS_Ci.nsIClassInfo.SINGLETON,
  implementationLanguage: AUS_Ci.nsIProgrammingLanguage.JAVASCRIPT,
  getHelperForLanguage: function(aLanguage) null,
  getInterfaces: function(aCount) {
    var interfaces = [AUS_Ci.nsISupports, AUS_Ci.nsIUpdatePrompt];
    aCount.value = interfaces.length;
    return interfaces;
  },
  classDescription: "UpdatePrompt",
  contractID: "@mozilla.org/updates/update-prompt;1",
  classID: Components.ID("{8c350a15-9b90-4622-93a1-4d320308664b}"),
  createInstance: function(aOuter, aIID) {
    if (aOuter == null)
      return gUpdatePrompt.QueryInterface(aIID);
    throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(AUS_Ci.nsIClassInfo) ||
        aIID.equals(AUS_Ci.nsISupports) ||
        aIID.equals(AUS_Ci.nsIUpdatePrompt))
      return gUpdatePrompt;
    throw AUS_Cr.NS_ERROR_NO_INTERFACE;
  },
};

/* Update check listener */
const updateCheckListener = {
  onProgress: function UCL_onProgress(aRequest, aPosition, aTotalSize) {
  },

  onCheckComplete: function UCL_onCheckComplete(aRequest, aUpdates, aUpdateCount) {
    gRequestURL = aRequest.channel.originalURI.spec;
    gUpdateCount = aUpdateCount;
    gUpdates = aUpdates;
    logTestInfo("url = " + gRequestURL + ", " +
                "request.status = " + aRequest.status + ", " +
                "update.statusText = " + aRequest.statusText + ", " +
                "updateCount = " + aUpdateCount);
    // Use a timeout to allow the XHR to complete
    do_execute_soon(gCheckFunc);
  },

  onError: function UCL_onError(aRequest, aUpdate) {
    gRequestURL = aRequest.channel.originalURI.spec;
    gStatusCode = aRequest.status;

    gStatusText = aUpdate.statusText;
    logTestInfo("url = " + gRequestURL + ", " +
                "request.status = " + gStatusCode + ", " +
                "update.statusText = " + gStatusText);
    // Use a timeout to allow the XHR to complete
    do_execute_soon(gCheckFunc.bind(null, aRequest, aUpdate));
  },

  QueryInterface: function(aIID) {
    if (!aIID.equals(AUS_Ci.nsIUpdateCheckListener) &&
        !aIID.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/* Update download listener - nsIRequestObserver */
const downloadListener = {
  onStartRequest: function DL_onStartRequest(aRequest, aContext) {
  },

  onProgress: function DL_onProgress(aRequest, aContext, aProgress, aMaxProgress) {
  },

  onStatus: function DL_onStatus(aRequest, aContext, aStatus, aStatusText) {
  },

  onStopRequest: function DL_onStopRequest(aRequest, aContext, aStatus) {
    gStatusResult = aStatus;
    // Use a timeout to allow the request to complete
    do_execute_soon(gCheckFunc);
  },

  QueryInterface: function DL_QueryInterface(aIID) {
    if (!aIID.equals(AUS_Ci.nsIRequestObserver) &&
        !aIID.equals(AUS_Ci.nsIProgressEventSink) &&
        !aIID.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

/**
 * Helper for starting the http server used by the tests
 */
function start_httpserver() {
  let dir = getTestDirFile();
  logTestInfo("http server directory path: " + dir.path);

  if (!dir.isDirectory()) {
    do_throw("A file instead of a directory was specified for HttpServer " +
             "registerDirectory! Path: " + dir.path + "\n");
  }

  AUS_Cu.import("resource://testing-common/httpd.js");
  gTestserver = new HttpServer();
  gTestserver.registerDirectory("/", dir);
  gTestserver.start(-1);
  let testserverPort = gTestserver.identity.primaryPort;
  gURLData = URL_HOST + ":" + testserverPort + "/";
  logTestInfo("http server port = " + testserverPort);
}

/**
 * Helper for stopping the http server used by the tests
 *
 * @param   aCallback
 *          The callback to call after stopping the http server.
 */
function stop_httpserver(aCallback) {
  do_check_true(!!aCallback);
  gTestserver.stop(aCallback);
}

/**
 * Creates an nsIXULAppInfo
 *
 * @param   aID
 *          The ID of the test application
 * @param   aName
 *          A name for the test application
 * @param   aVersion
 *          The version of the application
 * @param   aPlatformVersion
 *          The gecko version of the application
 */
function createAppInfo(aID, aName, aVersion, aPlatformVersion) {
  const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
  const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");
  var XULAppInfo = {
    vendor: APP_INFO_VENDOR,
    name: aName,
    ID: aID,
    version: aVersion,
    appBuildID: "2007010101",
    platformVersion: aPlatformVersion,
    platformBuildID: "2007010101",
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",

    QueryInterface: function QueryInterface(aIID) {
      if (aIID.equals(AUS_Ci.nsIXULAppInfo) ||
          aIID.equals(AUS_Ci.nsIXULRuntime) ||
#ifdef XP_WIN
          aIID.equals(AUS_Ci.nsIWinAppHelper) ||
#endif
          aIID.equals(AUS_Ci.nsISupports))
        return this;
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    }
  };

  var XULAppInfoFactory = {
    createInstance: function (aOuter, aIID) {
      if (aOuter == null)
        return XULAppInfo.QueryInterface(aIID);
      throw AUS_Cr.NS_ERROR_NO_AGGREGATION;
    }
  };

  var registrar = Components.manager.QueryInterface(AUS_Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

/**
 * Returns the platform specific arguments used by nsIProcess when launching
 * the application.
 *
 * @param   aExtraArgs (optional)
 *          An array of extra arguments to append to the default arguments.
 * @return  an array of arguments to be passed to nsIProcess.
 *
 * Note: a shell is necessary to pipe the application's console output which
 *       would otherwise pollute the xpcshell log.
 *
 * Command line arguments used when launching the application:
 * -no-remote prevents shell integration from being affected by an existing
 * application process.
 * -process-updates makes the application exits after being relaunched by the
 * updater.
 * the platform specific string defined by PIPE_TO_NULL to output both stdout
 * and stderr to null. This is needed to prevent output from the application
 * from ending up in the xpchsell log.
 */
function getProcessArgs(aExtraArgs) {
  if (!aExtraArgs) {
    aExtraArgs = [];
  }

  let appBinPath = getApplyDirFile(DIR_BIN_REL_PATH + FILE_APP_BIN, false).path;
  if (/ /.test(appBinPath)) {
    appBinPath = '"' + appBinPath + '"';
  }

  let args;
  if (IS_UNIX) {
    let launchScript = getLaunchScript();
    // Precreate the script with executable permissions
    launchScript.create(AUS_Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_DIRECTORY);

    let scriptContents = "#! /bin/sh\n";
    scriptContents += appBinPath + " -no-remote -process-updates " +
                      aExtraArgs.join(" ") + " " + PIPE_TO_NULL;
    writeFile(launchScript, scriptContents);
    logTestInfo("created " + launchScript.path + " containing:\n" +
                scriptContents);
    args = [launchScript.path];
  } else {
    args = ["/D", "/Q", "/C", appBinPath, "-no-remote", "-process-updates"].
           concat(aExtraArgs).concat([PIPE_TO_NULL]);
  }
  return args;
}

/**
 * Gets a file path for the application to dump its arguments into.  This is used
 * to verify that a callback application is launched.
 *
 * @return  the file for the application to dump its arguments into.
 */
function getAppArgsLogPath() {
  let appArgsLog = do_get_file("/", true);
  appArgsLog.append(gTestID + "_app_args_log");
  if (appArgsLog.exists()) {
    appArgsLog.remove(false);
  }
  let appArgsLogPath = appArgsLog.path;
  if (/ /.test(appArgsLogPath)) {
    appArgsLogPath = '"' + appArgsLogPath + '"';
  }
  return appArgsLogPath;
}

/**
 * Gets the nsIFile reference for the shell script to launch the application. If
 * the file exists it will be removed by this function.
 *
 * @return  the nsIFile for the shell script to launch the application.
 */
function getLaunchScript() {
  let launchScript = do_get_file("/", true);
  launchScript.append(gTestID + "_launch.sh");
  if (launchScript.exists()) {
    launchScript.remove(false);
  }
  return launchScript;
}

/**
 * Makes GreD, XREExeF, and UpdRootD point to unique file system locations so
 * xpcshell tests can run in parallel and to keep the environment clean.
 */
function adjustGeneralPaths() {
  let dirProvider = {
    getFile: function AGP_DP_getFile(aProp, aPersistent) {
      aPersistent.value = true;
      switch (aProp) {
        case NS_GRE_DIR:
          if (gUseTestAppDir) {
            return getApplyDirFile(DIR_BIN_REL_PATH, true);
          }
          break;
        case XRE_EXECUTABLE_FILE:
          if (gUseTestAppDir) {
            return getApplyDirFile(DIR_BIN_REL_PATH + FILE_APP_BIN, true);
          }
          break;
        case XRE_UPDATE_ROOT_DIR:
          return getMockUpdRootD();
      }
      return null;
    },
    QueryInterface: function(aIID) {
      if (aIID.equals(AUS_Ci.nsIDirectoryServiceProvider) ||
          aIID.equals(AUS_Ci.nsISupports))
        return this;
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    }
  };
  let ds = Services.dirsvc.QueryInterface(AUS_Ci.nsIDirectoryService);
  ds.QueryInterface(AUS_Ci.nsIProperties).undefine(NS_GRE_DIR);
  ds.QueryInterface(AUS_Ci.nsIProperties).undefine(XRE_EXECUTABLE_FILE);
  ds.registerProvider(dirProvider);
  do_register_cleanup(function AGP_cleanup() {
    logTestInfo("start - unregistering directory provider");

    if (gAppTimer) {
      logTestInfo("start - cancel app timer");
      gAppTimer.cancel();
      gAppTimer = null;
      logTestInfo("finish - cancel app timer");
    }

    if (gProcess && gProcess.isRunning) {
      logTestInfo("start - kill process");
      try {
        gProcess.kill();
      } catch (e) {
        logTestInfo("kill process failed. Exception: " + e);
      }
      gProcess = null;
      logTestInfo("finish - kill process");
    }

    if (gHandle) {
      try {
        logTestInfo("start - closing handle");
        let kernel32 = ctypes.open("kernel32");
        let CloseHandle = kernel32.declare("CloseHandle", ctypes.default_abi,
                                           ctypes.bool, /*return*/
                                           ctypes.voidptr_t /*handle*/);
        if (!CloseHandle(gHandle)) {
          logTestInfo("call to CloseHandle failed");
        }
        kernel32.close();
        gHandle = null;
        logTestInfo("finish - closing handle");
      } catch (e) {
        logTestInfo("call to CloseHandle failed. Exception: " + e);
      }
    }

    // Call end_test first before the directory provider is unregistered
    if (typeof(end_test) == typeof(Function)) {
      logTestInfo("calling end_test");
      end_test();
    }

    ds.unregisterProvider(dirProvider);
    cleanupTestCommon();

    logTestInfo("finish - unregistering directory provider");
  });
}


/**
 * Helper function for launching the application to apply an update.
 */
function launchAppToApplyUpdate() {
  logTestInfo("start - launching application to apply update");

  let appBin = getApplyDirFile(DIR_BIN_REL_PATH + FILE_APP_BIN, false);

  if (typeof(customLaunchAppToApplyUpdate) == typeof(Function)) {
    customLaunchAppToApplyUpdate();
  }

  let launchBin = getLaunchBin();
  let args = getProcessArgs();
  logTestInfo("launching " + launchBin.path + " " + args.join(" "));

  gProcess = AUS_Cc["@mozilla.org/process/util;1"].
                createInstance(AUS_Ci.nsIProcess);
  gProcess.init(launchBin);

  gAppTimer = AUS_Cc["@mozilla.org/timer;1"].createInstance(AUS_Ci.nsITimer);
  gAppTimer.initWithCallback(gTimerCallback, APP_TIMER_TIMEOUT,
                             AUS_Ci.nsITimer.TYPE_ONE_SHOT);

  setEnvironment();
  logTestInfo("launching application");
  gProcess.runAsync(args, args.length, gProcessObserver);
  resetEnvironment();

  logTestInfo("finish - launching application to apply update");
}

/**
 * The observer for the call to nsIProcess:runAsync.
 */
let gProcessObserver = {
  observe: function PO_observe(aSubject, aTopic, aData) {
    logTestInfo("topic: " + aTopic + ", process exitValue: " +
                gProcess.exitValue);
    if (gAppTimer) {
      gAppTimer.cancel();
      gAppTimer = null;
    }
    if (aTopic != "process-finished" || gProcess.exitValue != 0) {
      do_throw("Failed to launch application");
    }
    do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
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
      logTestInfo("attempt to kill process");
      gProcess.kill();
    }
    do_throw("launch application timer expired");
  },
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsITimerCallback])
};

/**
 * The update-staged observer for the call to nsIUpdateProcessor:processUpdate.
 */
let gUpdateStagedObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "update-staged") {
      Services.obs.removeObserver(gUpdateStagedObserver, "update-staged");
      checkUpdateApplied();
    }
  },
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsIObserver])
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
    let appGreDir = gGREDirOrig.clone();
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
    } else {
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
  } else {
    logTestInfo("setting the XPCOM_DEBUG_BREAK environment variable to " +
                "warn... previously it didn't exist");
  }

  env.set("XPCOM_DEBUG_BREAK", "warn");

  if (gStageUpdate) {
    logTestInfo("setting the MOZ_UPDATE_STAGING environment variable to 1\n");
    env.set("MOZ_UPDATE_STAGING", "1");
  }

  logTestInfo("setting MOZ_NO_SERVICE_FALLBACK environment variable to 1");
  env.set("MOZ_NO_SERVICE_FALLBACK", "1");
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
  } else {
    logTestInfo("clearing the XPCOM_DEBUG_BREAK environment variable");
    env.set("XPCOM_DEBUG_BREAK", "");
  }

  if (IS_UNIX) {
    if (IS_MACOSX) {
      if (gEnvDyldLibraryPath) {
        logTestInfo("setting DYLD_LIBRARY_PATH environment variable value " +
                    "back to " + gEnvDyldLibraryPath);
        env.set("DYLD_LIBRARY_PATH", gEnvDyldLibraryPath);
      } else {
        logTestInfo("removing DYLD_LIBRARY_PATH environment variable");
        env.set("DYLD_LIBRARY_PATH", "");
      }
    } else {
      if (gEnvLdLibraryPath) {
        logTestInfo("setting LD_LIBRARY_PATH environment variable value back " +
                    "to " + gEnvLdLibraryPath);
        env.set("LD_LIBRARY_PATH", gEnvLdLibraryPath);
      } else {
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

  if (gStageUpdate) {
    logTestInfo("removing the MOZ_UPDATE_STAGING environment variable\n");
    env.set("MOZ_UPDATE_STAGING", "");
  }

  logTestInfo("removing MOZ_NO_SERVICE_FALLBACK environment variable");
  env.set("MOZ_NO_SERVICE_FALLBACK", "");
}
