/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test log warnings that happen before the test has started
 * "Couldn't get the user appdata directory. Crash events may not be produced."
 * in nsExceptionHandler.cpp (possibly bug 619104)
 *
 * Test log warnings that happen after the test has finished
 * "OOPDeinit() without successful OOPInit()" in nsExceptionHandler.cpp
 * (bug 619104)
 * "XPCOM objects created/destroyed from static ctor/dtor" in nsTraceRefcnt.cpp
 * (possibly bug 457479)
 *
 * Other warnings printed to the test logs
 * "site security information will not be persisted" in
 * nsSiteSecurityService.cpp and the error in nsSystemInfo.cpp preceding this
 * error are due to not having a profile when running some of the xpcshell
 * tests. Since most xpcshell tests also log these errors these tests don't
 * call do_get_profile unless necessary for the test.
 * The "This method is lossy. Use GetCanonicalPath !" warning on Windows in
 * nsLocalFileWin.cpp is from the call to GetNSSProfilePath in
 * nsNSSComponent.cpp due to it using GetNativeCanonicalPath.
 * "!mMainThread" in nsThreadManager.cpp are due to using timers and it might be
 * possible to fix some or all of these in the test itself.
 * "NS_FAILED(rv)" in nsThreadUtils.cpp are due to using timers and it might be
 * possible to fix some or all of these in the test itself.
 */

'use strict';

const { classes: Cc, interfaces: Ci, manager: Cm, results: Cr,
        utils: Cu } = Components;

load("../data/xpcshellConstantsPP.js");

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/ctypes.jsm", this);

const DIR_MACOS = IS_MACOSX ? "Contents/MacOS/" : "";
const DIR_RESOURCES = IS_MACOSX ? "Contents/Resources/" : "";
const TEST_FILE_SUFFIX =  IS_MACOSX ? "_mac" : "";
const FILE_COMPLETE_MAR = "complete" + TEST_FILE_SUFFIX + ".mar";
const FILE_PARTIAL_MAR = "partial" + TEST_FILE_SUFFIX + ".mar";
const LOG_COMPLETE_SUCCESS = "complete_log_success" + TEST_FILE_SUFFIX;
const LOG_PARTIAL_SUCCESS  = "partial_log_success" + TEST_FILE_SUFFIX;
const LOG_PARTIAL_FAILURE  = "partial_log_failure" + TEST_FILE_SUFFIX;
const FILE_COMPLETE_PRECOMPLETE = "complete_precomplete" + TEST_FILE_SUFFIX;
const FILE_PARTIAL_PRECOMPLETE = "partial_precomplete" + TEST_FILE_SUFFIX;
const FILE_COMPLETE_REMOVEDFILES = "complete_removed-files" + TEST_FILE_SUFFIX;
const FILE_PARTIAL_REMOVEDFILES = "partial_removed-files" + TEST_FILE_SUFFIX;

const USE_EXECV = IS_UNIX && !IS_MACOSX;

const URL_HOST = "http://localhost";

const FILE_APP_BIN = MOZ_APP_NAME + APP_BIN_SUFFIX;
const FILE_COMPLETE_EXE = "complete.exe";
const FILE_HELPER_BIN = "TestAUSHelper" + BIN_SUFFIX;
const FILE_MAINTENANCE_SERVICE_BIN = "maintenanceservice.exe";
const FILE_MAINTENANCE_SERVICE_INSTALLER_BIN = "maintenanceservice_installer.exe";
const FILE_OLD_VERSION_MAR = "old_version.mar";
const FILE_PARTIAL_EXE = "partial.exe";
const FILE_UPDATER_BIN = "updater" + BIN_SUFFIX;
const FILE_WRONG_CHANNEL_MAR = "wrong_product_channel.mar";

const LOG_SWITCH_SUCCESS = "rename_file: proceeding to rename the directory\n" +
                           "rename_file: proceeding to rename the directory\n" +
                           "Now, remove the tmpDir\n" +
                           "succeeded\n" +
                           "calling QuitProgressUI";

const ERR_RENAME_FILE = "rename_file: failed to rename file";
const ERR_UNABLE_OPEN_DEST = "unable to open destination file";
const ERR_BACKUP_DISCARD = "backup_discard: unable to remove";
const ERR_MOVE_DESTDIR_7 = "Moving destDir to tmpDir failed, err: 7";

const LOG_SVC_SUCCESSFUL_LAUNCH = "Process was started... waiting on result.";

// Typical end of a message when calling assert
const MSG_SHOULD_EQUAL = " should equal the expected value";
const MSG_SHOULD_EXIST = "the file or directory should exist";
const MSG_SHOULD_NOT_EXIST = "the file or directory should not exist";

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

const PIPE_TO_NULL = IS_WIN ? ">nul" : "> /dev/null 2>&1";

const LOG_FUNCTION = do_print;

// This default value will be overridden when using the http server.
var gURLData = URL_HOST + "/";

var gTestID;

var gTestserver;

var gRegisteredServiceCleanup;

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
var gGREBinDirOrig;
var gAppDirOrig;

var gServiceLaunchedCallbackLog = null;
var gServiceLaunchedCallbackArgs = null;

// Variables are used instead of contants so tests can override these values if
// necessary.
var gCallbackBinFile = "callback_app" + BIN_SUFFIX;
var gCallbackArgs = ["./", "callback.log", "Test Arg 2", "Test Arg 3"];
var gPostUpdateBinFile = "postup_app" + BIN_SUFFIX;
var gStageUpdate = false;
var gSwitchApp = false;
var gUseTestAppDir = true;

var gTimeoutRuns = 0;

// Environment related globals
var gShouldResetEnv = undefined;
var gAddedEnvXRENoWindowsCrashDialog = false;
var gEnvXPCOMDebugBreak;
var gEnvXPCOMMemLeakLog;
var gEnvDyldLibraryPath;
var gEnvLdLibraryPath;

// Set to true to log additional information for debugging. To log additional
// information for an individual test set DEBUG_AUS_TEST to true in the test's
// run_test function.
var DEBUG_AUS_TEST = false;
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

const DATA_URI_SPEC = Services.io.newFileURI(do_get_file("../data", false)).spec;
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "shared.js", this);

var gTestFiles = [];
var gTestDirs = [];

// Common files for both successful and failed updates.
var gTestFilesCommon = [
{
  description      : "Should never change",
  fileName         : FILE_UPDATE_SETTINGS_INI,
  relPathDir       : DIR_RESOURCES,
  originalContents : UPDATE_SETTINGS_CONTENTS,
  compareContents  : UPDATE_SETTINGS_CONTENTS,
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o767,
  comparePerms     : 0o767
}, {
  description      : "Should never change",
  fileName         : "channel-prefs.js",
  relPathDir       : DIR_RESOURCES + "defaults/pref/",
  originalContents : "ShouldNotBeReplaced\n",
  compareContents  : "ShouldNotBeReplaced\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o767,
  comparePerms     : 0o767
}];

// Files for a complete successful update. This can be used for a complete
// failed update by calling setTestFilesAndDirsForFailure.
var gTestFilesCompleteSuccess = [
{
  description      : "Added by update.manifest (add)",
  fileName         : "precomplete",
  relPathDir       : DIR_RESOURCES,
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_PARTIAL_PRECOMPLETE,
  compareFile      : FILE_COMPLETE_PRECOMPLETE,
  originalPerms    : 0o666,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginstext0",
  relPathDir       : DIR_RESOURCES + "searchplugins/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o775,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginspng1.png",
  relPathDir       : DIR_RESOURCES + "searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png",
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginspng0.png",
  relPathDir       : DIR_RESOURCES + "searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "complete.png",
  originalPerms    : 0o666,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "removed-files",
  relPathDir       : DIR_RESOURCES,
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_PARTIAL_REMOVEDFILES,
  compareFile      : FILE_COMPLETE_REMOVEDFILES,
  originalPerms    : 0o666,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1text0",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions1/",
  originalContents : null,
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1png1.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "partial.png",
  compareFile      : "complete.png",
  originalPerms    : 0o666,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1png0.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png",
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0text0",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions0/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0png1.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png",
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0png0.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png",
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "exe0.exe",
  relPathDir       : DIR_MACOS,
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_HELPER_BIN,
  compareFile      : FILE_COMPLETE_EXE,
  originalPerms    : 0o777,
  comparePerms     : 0o755
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "10text0",
  relPathDir       : DIR_RESOURCES + "1/10/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o767,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "0exe0.exe",
  relPathDir       : DIR_RESOURCES + "0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_HELPER_BIN,
  compareFile      : FILE_COMPLETE_EXE,
  originalPerms    : 0o777,
  comparePerms     : 0o755
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text1",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o677,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text0",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : "ToBeReplacedWithFromComplete\n",
  compareContents  : "FromComplete\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o775,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00png0.png",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "complete.png",
  originalPerms    : 0o776,
  comparePerms     : 0o644
}, {
  description      : "Removed by precomplete (remove)",
  fileName         : "20text0",
  relPathDir       : DIR_RESOURCES + "2/20/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : null
}, {
  description      : "Removed by precomplete (remove)",
  fileName         : "20png0.png",
  relPathDir       : DIR_RESOURCES + "2/20/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : null
}];

// Concatenate the common files to the end of the array.
gTestFilesCompleteSuccess = gTestFilesCompleteSuccess.concat(gTestFilesCommon);

// Files for a partial successful update. This can be used for a partial failed
// update by calling setTestFilesAndDirsForFailure.
var gTestFilesPartialSuccess = [
{
  description      : "Added by update.manifest (add)",
  fileName         : "precomplete",
  relPathDir       : DIR_RESOURCES,
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_COMPLETE_PRECOMPLETE,
  compareFile      : FILE_PARTIAL_PRECOMPLETE,
  originalPerms    : 0o666,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "searchpluginstext0",
  relPathDir       : DIR_RESOURCES + "searchplugins/",
  originalContents : "ToBeReplacedWithFromPartial\n",
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o775,
  comparePerms     : 0o644
}, {
  description      : "Patched by update.manifest if the file exists " +
                     "(patch-if)",
  fileName         : "searchpluginspng1.png",
  relPathDir       : DIR_RESOURCES + "searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o666,
  comparePerms     : 0o666
}, {
  description      : "Patched by update.manifest if the file exists " +
                     "(patch-if)",
  fileName         : "searchpluginspng0.png",
  relPathDir       : DIR_RESOURCES + "searchplugins/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o666,
  comparePerms     : 0o666
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions1text0",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions1/",
  originalContents : null,
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions1png1.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o666,
  comparePerms     : 0o666
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions1png0.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions1/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o666,
  comparePerms     : 0o666
}, {
  description      : "Added by update.manifest if the parent directory " +
                     "exists (add-if)",
  fileName         : "extensions0text0",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions0/",
  originalContents : "ToBeReplacedWithFromPartial\n",
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o644,
  comparePerms     : 0o644
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions0png1.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o644,
  comparePerms     : 0o644
}, {
  description      : "Patched by update.manifest if the parent directory " +
                     "exists (patch-if)",
  fileName         : "extensions0png0.png",
  relPathDir       : DIR_RESOURCES + "distribution/extensions/extensions0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o644,
  comparePerms     : 0o644
}, {
  description      : "Patched by update.manifest (patch)",
  fileName         : "exe0.exe",
  relPathDir       : DIR_MACOS,
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_COMPLETE_EXE,
  compareFile      : FILE_PARTIAL_EXE,
  originalPerms    : 0o755,
  comparePerms     : 0o755
}, {
  description      : "Patched by update.manifest (patch)",
  fileName         : "0exe0.exe",
  relPathDir       : DIR_RESOURCES + "0/",
  originalContents : null,
  compareContents  : null,
  originalFile     : FILE_COMPLETE_EXE,
  compareFile      : FILE_PARTIAL_EXE,
  originalPerms    : 0o755,
  comparePerms     : 0o755
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text0",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : "ToBeReplacedWithFromPartial\n",
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : 0o644,
  comparePerms     : 0o644
}, {
  description      : "Patched by update.manifest (patch)",
  fileName         : "00png0.png",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : null,
  compareContents  : null,
  originalFile     : "complete.png",
  compareFile      : "partial.png",
  originalPerms    : 0o666,
  comparePerms     : 0o666
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "20text0",
  relPathDir       : DIR_RESOURCES + "2/20/",
  originalContents : null,
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "20png0.png",
  relPathDir       : DIR_RESOURCES + "2/20/",
  originalContents : null,
  compareContents  : null,
  originalFile     : null,
  compareFile      : "partial.png",
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Added by update.manifest (add)",
  fileName         : "00text2",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : null,
  compareContents  : "FromPartial\n",
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : 0o644
}, {
  description      : "Removed by update.manifest (remove)",
  fileName         : "10text0",
  relPathDir       : DIR_RESOURCES + "1/10/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : null
}, {
  description      : "Removed by update.manifest (remove)",
  fileName         : "00text1",
  relPathDir       : DIR_RESOURCES + "0/00/",
  originalContents : "ToBeDeleted\n",
  compareContents  : null,
  originalFile     : null,
  compareFile      : null,
  originalPerms    : null,
  comparePerms     : null
}];

// Concatenate the common files to the end of the array.
gTestFilesPartialSuccess = gTestFilesPartialSuccess.concat(gTestFilesCommon);

var gTestDirsCommon = [
{
  relPathDir   : DIR_RESOURCES + "3/",
  dirRemoved   : false,
  files        : ["3text0", "3text1"],
  filesRemoved : true
}, {
  relPathDir   : DIR_RESOURCES + "4/",
  dirRemoved   : true,
  files        : ["4text0", "4text1"],
  filesRemoved : true
}, {
  relPathDir   : DIR_RESOURCES + "5/",
  dirRemoved   : true,
  files        : ["5test.exe", "5text0", "5text1"],
  filesRemoved : true
}, {
  relPathDir   : DIR_RESOURCES + "6/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "7/",
  dirRemoved   : true,
  files        : ["7text0", "7text1"],
  subDirs      : ["70/", "71/"],
  subDirFiles  : ["7xtest.exe", "7xtext0", "7xtext1"]
}, {
  relPathDir   : DIR_RESOURCES + "8/",
  dirRemoved   : false
}, {
  relPathDir   : DIR_RESOURCES + "8/80/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "8/81/",
  dirRemoved   : false,
  files        : ["81text0", "81text1"]
}, {
  relPathDir   : DIR_RESOURCES + "8/82/",
  dirRemoved   : false,
  subDirs      : ["820/", "821/"]
}, {
  relPathDir   : DIR_RESOURCES + "8/83/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "8/84/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "8/85/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "8/86/",
  dirRemoved   : true,
  files        : ["86text0", "86text1"]
}, {
  relPathDir   : DIR_RESOURCES + "8/87/",
  dirRemoved   : true,
  subDirs      : ["870/", "871/"],
  subDirFiles  : ["87xtext0", "87xtext1"]
}, {
  relPathDir   : DIR_RESOURCES + "8/88/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "8/89/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "9/90/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "9/91/",
  dirRemoved   : false,
  files        : ["91text0", "91text1"]
}, {
  relPathDir   : DIR_RESOURCES + "9/92/",
  dirRemoved   : false,
  subDirs      : ["920/", "921/"]
}, {
  relPathDir   : DIR_RESOURCES + "9/93/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "9/94/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "9/95/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "9/96/",
  dirRemoved   : true,
  files        : ["96text0", "96text1"]
}, {
  relPathDir   : DIR_RESOURCES + "9/97/",
  dirRemoved   : true,
  subDirs      : ["970/", "971/"],
  subDirFiles  : ["97xtext0", "97xtext1"]
}, {
  relPathDir   : DIR_RESOURCES + "9/98/",
  dirRemoved   : true
}, {
  relPathDir   : DIR_RESOURCES + "9/99/",
  dirRemoved   : true
}];

// Directories for a complete successful update. This array can be used for a
// complete failed update by calling setTestFilesAndDirsForFailure.
var gTestDirsCompleteSuccess = [
{
  description  : "Removed by precomplete (rmdir)",
  relPathDir   : DIR_RESOURCES + "2/20/",
  dirRemoved   : true
}, {
  description  : "Removed by precomplete (rmdir)",
  relPathDir   : DIR_RESOURCES + "2/",
  dirRemoved   : true
}];

// Concatenate the common files to the beginning of the array.
gTestDirsCompleteSuccess = gTestDirsCommon.concat(gTestDirsCompleteSuccess);

// Directories for a partial successful update. This array can be used for a
// partial failed update by calling setTestFilesAndDirsForFailure.
var gTestDirsPartialSuccess = [
{
  description  : "Removed by update.manifest (rmdir)",
  relPathDir   : DIR_RESOURCES + "1/10/",
  dirRemoved   : true
}, {
  description  : "Removed by update.manifest (rmdir)",
  relPathDir   : DIR_RESOURCES + "1/",
  dirRemoved   : true
}];

// Concatenate the common files to the beginning of the array.
gTestDirsPartialSuccess = gTestDirsCommon.concat(gTestDirsPartialSuccess);

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
  debugDump("start - general test setup");

  do_test_pending();

  Assert.strictEqual(gTestID, undefined,
                     "gTestID should be 'undefined' (setupTestCommon should " +
                     "only be called once)");

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
               "non-parallel log.");
    } else {
      gRealDump = dump;
      dump = dumpOverride;
    }
  }

  // Don't attempt to show a prompt when an update finishes.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, true);

  gGREDirOrig = getGREDir();
  gGREBinDirOrig = getGREBinDir();
  gAppDirOrig = getAppBaseDir();

  let applyDir = getApplyDirFile(null, true).parent;

  // Try to remove the directory used to apply updates and the updates directory
  // on platforms other than Windows. Since the test hasn't ran yet and the
  // directory shouldn't exist finished this is non-fatal for the test.
  if (applyDir.exists()) {
    debugDump("attempting to remove directory. Path: " + applyDir.path);
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

  // This prevents a warning about not being able to find the greprefs.js file
  // from being logged.
  let grePrefsFile = getGREDir();
  if (!grePrefsFile.exists()) {
    grePrefsFile.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  grePrefsFile.append("greprefs.js");
  if (!grePrefsFile.exists()) {
    grePrefsFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
  }

  // Remove the updates directory on Windows and Mac OS X which is located
  // outside of the application directory after the call to adjustGeneralPaths
  // has set it up. Since the test hasn't ran yet and the directory shouldn't
  // exist this is non-fatal for the test.
  if (IS_WIN || IS_MACOSX) {
    let updatesDir = getMockUpdRootD();
    if (updatesDir.exists()) {
      debugDump("attempting to remove directory. Path: " + updatesDir.path);
      try {
        removeDirRecursive(updatesDir);
      } catch (e) {
        logTestInfo("non-fatal error removing directory. Path: " +
                    updatesDir.path + ", Exception: " + e);
      }
    }
  }

  debugDump("finish - general test setup");
}

/**
 * Nulls out the most commonly used global vars used by tests to prevent leaks
 * as needed and attempts to restore the system to its original state.
 */
function cleanupTestCommon() {
  debugDump("start - general test cleanup");

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
    let key = Cc["@mozilla.org/windows-registry-key;1"].
              createInstance(Ci.nsIWindowsRegKey);
    try {
      key.open(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, REG_PATH,
               Ci.nsIWindowsRegKey.ACCESS_ALL);
      if (key.hasValue(appDir.path)) {
        key.removeValue(appDir.path);
      }
    } catch (e) {
    }
    try {
      key.open(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, REG_PATH,
               Ci.nsIWindowsRegKey.ACCESS_ALL);
      if (key.hasValue(appDir.path)) {
        key.removeValue(appDir.path);
      }
    } catch (e) {
    }
  }

  // The updates directory is located outside of the application directory on
  // Windows and Mac OS X so it also needs to be removed.
  if (IS_WIN || IS_MACOSX) {
    let updatesDir = getMockUpdRootD();
    // Try to remove the directory used to apply updates. Since the test has
    // already finished this is non-fatal for the test.
    if (updatesDir.exists()) {
      debugDump("attempting to remove directory. Path: " + updatesDir.path);
      try {
        removeDirRecursive(updatesDir);
      } catch (e) {
        logTestInfo("non-fatal error removing directory. Path: " +
                    updatesDir.path + ", Exception: " + e);
      }
      if (IS_MACOSX) {
        let updatesRootDir = gUpdatesRootDir.clone();
        while (updatesRootDir.path != updatesDir.path) {
          if (updatesDir.exists()) {
            debugDump("attempting to remove directory. Path: " +
                      updatesDir.path);
            try {
              // Try to remove the directory without the recursive flag set
              // since the top level directory has already had its contents
              // removed and the parent directory might still be used by a
              // different test.
              updatesDir.remove(false);
            } catch (e) {
              logTestInfo("non-fatal error removing directory. Path: " +
                          updatesDir.path + ", Exception: " + e);
            }
          }
          updatesDir = updatesDir.parent;
        }
      }
    }
  }

  let applyDir = getApplyDirFile(null, true).parent;

  // Try to remove the directory used to apply updates. Since the test has
  // already finished this is non-fatal for the test.
  if (applyDir.exists()) {
    debugDump("attempting to remove directory. Path: " + applyDir.path);
    try {
      removeDirRecursive(applyDir);
    } catch (e) {
      logTestInfo("non-fatal error removing directory. Path: " +
                  applyDir.path + ", Exception: " + e);
    }
  }

  resetEnvironment();

  debugDump("finish - general test cleanup");

  if (gRealDump) {
    dump = gRealDump;
    gRealDump = null;
  }

  if (DEBUG_TEST_LOG && !gPassed) {
    let fos = Cc["@mozilla.org/network/file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);
    let logFile = do_get_file(gTestID + ".log", true);
    if (!logFile.exists()) {
      logFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_FILE);
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
  if (DEBUG_AUS_TEST) {
    // This prevents do_print errors from being printed by the xpcshell test
    // harness due to nsUpdateService.js logging to the console when the
    // app.update.log preference is true.
    Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG, false);
    gAUS.observe(null, "nsPref:changed", PREF_APP_UPDATE_LOG);
  }
  do_execute_soon(do_test_finished);
}

/**
 * Sets the most commonly used preferences used by tests
 */
function setDefaultPrefs() {
  Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, true);
  // Don't display UI for a successful installation. Some apps may not set this
  // pref to false like Firefox does.
  Services.prefs.setBoolPref(PREF_APP_UPDATE_SHOW_INSTALLED_UI, false);
  if (DEBUG_AUS_TEST) {
    // Enable Update logging
    Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG, true);
  }
}

/**
 * Helper function for updater binary tests that sets the appropriate values
 * to check for update failures.
 */
function setTestFilesAndDirsForFailure() {
  gTestFiles.forEach(function STFADFF_Files(aTestFile) {
    aTestFile.compareContents = aTestFile.originalContents;
    aTestFile.compareFile = aTestFile.originalFile;
    aTestFile.comparePerms = aTestFile.originalPerms;
  });

  gTestDirs.forEach(function STFADFF_Dirs(aTestDir) {
    aTestDir.dirRemoved = false;
    if (aTestDir.filesRemoved) {
      aTestDir.filesRemoved = false;
    }
  });
}

/**
 * Helper function for updater binary tests that prevents the distribution
 * directory files from being created.
 */
function preventDistributionFiles() {
  gTestFiles = gTestFiles.filter(function(aTestFile) {
    return aTestFile.relPathDir.indexOf("distribution/") == -1;
  });

  gTestDirs = gTestDirs.filter(function(aTestDir) {
    return aTestDir.relPathDir.indexOf("distribution/") == -1;
  });
}

/**
 * On Mac OS X this sets the last modified time for the app bundle directory to
 * a date in the past to test that the last modified time is updated when an
 * update has been successfully applied (bug 600098).
 */
function setAppBundleModTime() {
  if (!IS_MACOSX) {
    return;
  }
  let now = Date.now();
  let yesterday = now - (1000 * 60 * 60 * 24);
  let applyToDir = getApplyDirFile();
  applyToDir.lastModifiedTime = yesterday;
}

/**
 * On Mac OS X this checks that the last modified time for the app bundle
 * directory has been updated when an update has been successfully applied
 * (bug 600098).
 */
function checkAppBundleModTime() {
  if (!IS_MACOSX) {
    return;
  }
  let now = Date.now();
  let applyToDir = getApplyDirFile();
  let timeDiff = Math.abs(applyToDir.lastModifiedTime - now);
  Assert.ok(timeDiff < MAC_MAX_TIME_DIFFERENCE,
            "the last modified time on the apply to directory should " +
            "change after a successful update");
}

/**
 * On Mac OS X and Windows this checks if the post update '.running' file exists
 * to determine if the post update binary was launched.
 *
 * @param   aShouldExist
 *          Whether the post update '.running' file should exist.
 */
function checkPostUpdateRunningFile(aShouldExist) {
  if (!IS_WIN && !IS_MACOSX) {
    return;
  }
  let postUpdateRunningFile = getPostUpdateFile(".running");
  if (aShouldExist) {
    Assert.ok(postUpdateRunningFile.exists(), MSG_SHOULD_EXIST);
  } else {
    Assert.ok(!postUpdateRunningFile.exists(), MSG_SHOULD_NOT_EXIST);
  }
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
 */
function getAppVersion() {
  // Read the application.ini and use its application version.
  let iniFile = gGREDirOrig.clone();
  iniFile.append(FILE_APPLICATION_INI);
  if (!iniFile.exists()) {
    iniFile = gGREBinDirOrig.clone();
    iniFile.append(FILE_APPLICATION_INI);
  }
  Assert.ok(iniFile.exists(), MSG_SHOULD_EXIST);
  let iniParser = Cc["@mozilla.org/xpcom/ini-parser-factory;1"].
                  getService(Ci.nsIINIParserFactory).
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
 * @throws  If aAllowNonexistent is not specified or is false and the file or
 *          directory does not exist.
 */
function getApplyDirFile(aRelPath, aAllowNonexistent) {
  let relpath = getApplyDirPath() + (aRelPath ? aRelPath : "");
  return do_get_file(relpath, aAllowNonexistent);
}

/**
 * Helper function for getting the nsIFile for a file in the directory where the
 * update will be staged.
 *
 * The files for the update are located two directories below the stage
 * directory since Mac OS X sets the last modified time for the root directory
 * to the current time and if the update changes any files in the root directory
 * then it wouldn't be possible to test (bug 600098).
 *
 * @param   aRelPath (optional)
 *          The relative path to the file or directory to get from the root of
 *          the stage directory. If not specified the stage directory will be
 *          returned.
 * @param   aAllowNonexistent (optional)
 *          Whether the file must exist. If false or not specified the file must
 *          exist or the function will throw.
 * @return  The nsIFile for the file in the directory where the update will be
 *          staged.
 * @throws  If aAllowNonexistent is not specified or is false and the file or
 *          directory does not exist.
 */
function getStageDirFile(aRelPath, aAllowNonexistent) {
  if (IS_MACOSX) {
    let file = getMockUpdRootD();
    file.append(DIR_UPDATES);
    file.append(DIR_PATCH);
    file.append(DIR_UPDATED);
    if (aRelPath) {
      let pathParts = aRelPath.split("/");
      for (let i = 0; i < pathParts.length; i++) {
        if (pathParts[i]) {
          file.append(pathParts[i]);
        }
      }
    }
    if (!aAllowNonexistent) {
      Assert.ok(file.exists(), MSG_SHOULD_EXIST);
    }
    return file;
  }

  let relpath = getApplyDirPath() + DIR_UPDATED + "/" + (aRelPath ? aRelPath : "");
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
 * @param   aAllowNonExists (optional)
 *          Whether or not to throw an error if the path exists.
 *          If not specified, then false is used.
 * @return  The nsIFile for the file in the test data directory.
 * @throws  If the file or directory does not exist.
 */
function getTestDirFile(aRelPath, aAllowNonExists) {
  let relpath = getTestDirPath() + (aRelPath ? aRelPath : "");
  return do_get_file(relpath, !!aAllowNonExists);
}

/**
 * Helper function for getting the nsIFile for the maintenance service
 * directory on Windows.
 *
 * @return  The nsIFile for the maintenance service directory.
 */
function getMaintSvcDir() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  const CSIDL_PROGRAM_FILES = 0x26;
  const CSIDL_PROGRAM_FILESX86 = 0x2A;
  // This will return an empty string on our Win XP build systems.
  let maintSvcDir = getSpecialFolderDir(CSIDL_PROGRAM_FILESX86);
  if (maintSvcDir) {
    maintSvcDir.append("Mozilla Maintenance Service");
    debugDump("using CSIDL_PROGRAM_FILESX86 - maintenance service install " +
              "directory path: " + maintSvcDir.path);
  }
  if (!maintSvcDir || !maintSvcDir.exists()) {
    maintSvcDir = getSpecialFolderDir(CSIDL_PROGRAM_FILES);
    if (maintSvcDir) {
      maintSvcDir.append("Mozilla Maintenance Service");
      debugDump("using CSIDL_PROGRAM_FILES - maintenance service install " +
                "directory path: " + maintSvcDir.path);
    }
  }
  if (!maintSvcDir) {
    do_throw("Unable to find the maintenance service install directory");
  }

  return maintSvcDir;
}

function getSpecialFolderDir(aCSIDL) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let lib = ctypes.open("shell32");
  let SHGetSpecialFolderPath = lib.declare("SHGetSpecialFolderPathW",
                                           ctypes.winapi_abi,
                                           ctypes.bool, /* bool(return) */
                                           ctypes.int32_t, /* HWND hwndOwner */
                                           ctypes.char16_t.ptr, /* LPTSTR lpszPath */
                                           ctypes.int32_t, /* int csidl */
                                           ctypes.bool /* BOOL fCreate */);

  let aryPath = ctypes.char16_t.array()(260);
  let rv = SHGetSpecialFolderPath(0, aryPath, aCSIDL, false);
  lib.close();

  let path = aryPath.readString(); // Convert the c-string to js-string
  if (!path) {
    return null;
  }
  debugDump("SHGetSpecialFolderPath returned path: " + path);
  let dir = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  dir.initWithPath(path);
  return dir;
}

XPCOMUtils.defineLazyGetter(this, "gInstallDirPathHash",
                            function test_gInstallDirPathHash() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  // Figure out where we should check for a cached hash value
  if (!MOZ_APP_BASENAME) {
    return null;
  }

  let vendor = MOZ_APP_VENDOR ? MOZ_APP_VENDOR : "Mozilla";
  let appDir = getApplyDirFile(null, true);

  const REG_PATH = "SOFTWARE\\" + vendor + "\\" + MOZ_APP_BASENAME +
                   "\\TaskBarIDs";
  let regKey = Cc["@mozilla.org/windows-registry-key;1"].
               createInstance(Ci.nsIWindowsRegKey);
  try {
    regKey.open(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, REG_PATH,
                Ci.nsIWindowsRegKey.ACCESS_ALL);
    regKey.writeStringValue(appDir.path, gTestID);
    return gTestID;
  } catch (e) {
  }

  try {
    regKey.create(Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER, REG_PATH,
                  Ci.nsIWindowsRegKey.ACCESS_ALL);
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
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  const CSIDL_LOCAL_APPDATA = 0x1c;
  return getSpecialFolderDir(CSIDL_LOCAL_APPDATA);
});

XPCOMUtils.defineLazyGetter(this, "gProgFilesDir",
                            function test_gProgFilesDir() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  const CSIDL_PROGRAM_FILES = 0x26;
  return getSpecialFolderDir(CSIDL_PROGRAM_FILES);
});

/**
 * Helper function for getting the update root directory used by the tests. This
 * returns the same directory as returned by nsXREDirProvider::GetUpdateRootDir
 * in nsXREDirProvider.cpp so an application will be able to find the update
 * when running a test that launches the application.
 */
function getMockUpdRootD() {
  if (IS_WIN) {
    return getMockUpdRootDWin();
  }

  if (IS_MACOSX) {
    return getMockUpdRootDMac();
  }

  return getApplyDirFile(DIR_MACOS, true);
}

/**
 * Helper function for getting the update root directory used by the tests. This
 * returns the same directory as returned by nsXREDirProvider::GetUpdateRootDir
 * in nsXREDirProvider.cpp so an application will be able to find the update
 * when running a test that launches the application.
 */
function getMockUpdRootDWin() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let localAppDataDir = gLocalAppDataDir.clone();
  let progFilesDir = gProgFilesDir.clone();
  let appDir = Services.dirsvc.get(XRE_EXECUTABLE_FILE, Ci.nsIFile).parent;

  let appDirPath = appDir.path;
  let relPathUpdates = "";
  if (gInstallDirPathHash && (MOZ_APP_VENDOR || MOZ_APP_BASENAME)) {
    relPathUpdates += (MOZ_APP_VENDOR ? MOZ_APP_VENDOR : MOZ_APP_BASENAME) +
                      "\\" + DIR_UPDATES + "\\" + gInstallDirPathHash;
  }

  if (!relPathUpdates && progFilesDir) {
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

  let updatesDir = Cc["@mozilla.org/file/local;1"].
                   createInstance(Ci.nsILocalFile);
  updatesDir.initWithPath(localAppDataDir.path + "\\" + relPathUpdates);
  debugDump("returning UpdRootD Path: " + updatesDir.path);
  return updatesDir;
}

XPCOMUtils.defineLazyGetter(this, "gUpdatesRootDir",
                            function test_gUpdatesRootDir() {
  if (!IS_MACOSX) {
    do_throw("Mac OS X only function called by a different platform!");
  }

  let dir = Services.dirsvc.get("ULibDir", Ci.nsILocalFile);
  dir.append("Caches");
  if (MOZ_APP_VENDOR || MOZ_APP_BASENAME) {
    dir.append(MOZ_APP_VENDOR ? MOZ_APP_VENDOR : MOZ_APP_BASENAME);
  } else {
    dir.append("Mozilla");
  }
  dir.append(DIR_UPDATES);
  return dir;
});

/**
 * Helper function for getting the update root directory used by the tests. This
 * returns the same directory as returned by nsXREDirProvider::GetUpdateRootDir
 * in nsXREDirProvider.cpp so an application will be able to find the update
 * when running a test that launches the application.
 */
function getMockUpdRootDMac() {
  if (!IS_MACOSX) {
    do_throw("Mac OS X only function called by a different platform!");
  }

  let appDir = Services.dirsvc.get(XRE_EXECUTABLE_FILE, Ci.nsIFile).
               parent.parent.parent;
  let appDirPath = appDir.path;
  appDirPath = appDirPath.substr(0, appDirPath.length - 4);

  let pathUpdates = gUpdatesRootDir.path + appDirPath;
  let updatesDir = Cc["@mozilla.org/file/local;1"].
                   createInstance(Ci.nsILocalFile);
  updatesDir.initWithPath(pathUpdates);
  debugDump("returning UpdRootD Path: " + updatesDir.path);
  return updatesDir;
}

const kLockFileName = "updated.update_in_progress.lock";
/**
 * Helper function for locking a directory on Windows.
 *
 * @param   aDir
 *          The nsIFile for the directory to lock.
 */
function lockDirectory(aDir) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let file = aDir.clone();
  file.append(kLockFileName);
  file.create(file.NORMAL_FILE_TYPE, 0o444);
  file.QueryInterface(Ci.nsILocalFileWin);
  file.fileAttributesWin |= file.WFA_READONLY;
  file.fileAttributesWin &= ~file.WFA_READWRITE;
  Assert.ok(file.exists(), MSG_SHOULD_EXIST);
  Assert.ok(!file.isWritable(),
            "the lock file should not be writeable");
}
/**
 * Helper function for unlocking a directory on Windows.
 *
 * @param   aDir
 *          The nsIFile for the directory to unlock.
 */
function unlockDirectory(aDir) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }
  let file = aDir.clone();
  file.append(kLockFileName);
  file.QueryInterface(Ci.nsILocalFileWin);
  file.fileAttributesWin |= file.WFA_READWRITE;
  file.fileAttributesWin &= ~file.WFA_READONLY;
  file.remove(false);
  Assert.ok(!file.exists(), MSG_SHOULD_NOT_EXIST);
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
  let binDir = gGREBinDirOrig.clone();
  let updater = getTestDirFile("updater.app", true);
  if (!updater.exists()) {
    updater = getTestDirFile(FILE_UPDATER_BIN);
    if (!updater.exists()) {
      do_throw("Unable to find the updater binary!");
    }
  }
  Assert.ok(updater.exists(), MSG_SHOULD_EXIST);

  let updatesDir = getUpdatesPatchDir();
  let updateBin;
  if (IS_WIN) {
    updateBin = updater.clone();
  } else {
    updater.copyToFollowingLinks(updatesDir, updater.leafName);
    updateBin = updatesDir.clone();
    updateBin.append(updater.leafName);
    if (updateBin.leafName == "updater.app") {
      updateBin.append("Contents");
      updateBin.append("MacOS");
      updateBin.append("updater");
    }
  }
  Assert.ok(updateBin.exists(), MSG_SHOULD_EXIST);

  let applyToDir = getApplyDirFile(null, true);
  let applyToDirPath = applyToDir.path;

  let stageDir = getStageDirFile(null, true);
  let stageDirPath = stageDir.path;

  if (IS_WIN) {
    // Convert to native path
    applyToDirPath = applyToDirPath.replace(/\//g, "\\");
    stageDirPath = stageDirPath.replace(/\//g, "\\");
  }

  let callbackApp = getApplyDirFile(DIR_RESOURCES + gCallbackBinFile);
  callbackApp.permissions = PERMS_DIRECTORY;

  let args = [updatesDir.path, applyToDirPath];
  if (gStageUpdate) {
    args[2] = stageDirPath;
    args[3] = -1;
  } else {
    if (gSwitchApp) {
      args[2] = stageDirPath;
      args[3] = "0/replace";
    } else {
      args[2] = applyToDirPath;
      args[3] = "0";
    }
    args = args.concat([callbackApp.parent.path, callbackApp.path]);
    args = args.concat(gCallbackArgs);
  }
  debugDump("running the updater: " + updateBin.path + " " + args.join(" "));

  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
  process.init(updateBin);
  process.run(true, args, args.length);

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
    // xpcshell tests won't display the entire contents so log each line.
    let updateLogContents = readFileBytes(updateLog).replace(/\r\n/g, "\n");
    updateLogContents = replaceLogPaths(updateLogContents);
    let aryLogContents = updateLogContents.split("\n");
    logTestInfo("contents of " + updateLog.path + ":");
    aryLogContents.forEach(function RU_LC_FE(aLine) {
      logTestInfo(aLine);
    });
  }
  Assert.equal(process.exitValue, aExpectedExitValue,
               "the process exit value" + MSG_SHOULD_EQUAL);
  Assert.equal(status, aExpectedStatus,
               "the update status" + MSG_SHOULD_EQUAL);

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
  debugDump("start - attempting to stage update");
  Services.obs.addObserver(gUpdateStagedObserver, "update-staged", false);

  setEnvironment();
  // Stage the update.
  Cc["@mozilla.org/updates/update-processor;1"].
    createInstance(Ci.nsIUpdateProcessor).
    processUpdate(gUpdateManager.activeUpdate);
  resetEnvironment();

  debugDump("finish - attempting to stage update");
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
  let binDir = getGREBinDir();
  let updaterBin = binDir.clone();
  updaterBin.append(FILE_UPDATER_BIN);
  Assert.ok(updaterBin.exists(), MSG_SHOULD_EXIST);

  let updaterBinPath = updaterBin.path;
  if (/ /.test(updaterBinPath)) {
    updaterBinPath = '"' + updaterBinPath + '"';
  }

  let isBinSigned = isBinarySigned(updaterBinPath);

  const REG_PATH = "SOFTWARE\\Mozilla\\MaintenanceService\\" +
                   "3932ecacee736d366d6436db0f55bce4";
  let key = Cc["@mozilla.org/windows-registry-key;1"].
            createInstance(Ci.nsIWindowsRegKey);
  try {
    key.open(Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE, REG_PATH,
             Ci.nsIWindowsRegKey.ACCESS_READ | key.WOW64_64);
  } catch (e) {
    // The build system could sign the files and not have the test registry key
    // in which case we should fail the test if the updater binary is signed so
    // the build system can be fixed by adding the registry key.
    if (IS_AUTHENTICODE_CHECK_ENABLED) {
      Assert.ok(!isBinSigned,
                "the updater.exe binary should not be signed when the test " +
                "registry key doesn't exist (if it is, build system " +
                "configuration bug?)");
    }

    logTestInfo("this test can only run on the buildbot build system at this " +
                "time");
    return false;
  }

  // Check to make sure the service is installed
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let args = ["wait-for-service-stop", "MozillaMaintenance", "10"];
  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
  process.init(helperBin);
  process.run(true, args, args.length);
  Assert.notEqual(process.exitValue, 0xEE, "the maintenance service should " +
                  "be installed (if not, build system configuration bug?)");

  // If this is the first test in the series, then there is no reason the
  // service should be anything but stopped, so be strict here and fail the
  // test.
  if (aFirstTest) {
    Assert.equal(process.exitValue, 0,
                 "the service should not be running for the first test");
  }

  if (IS_AUTHENTICODE_CHECK_ENABLED) {
    // The test registry key exists and IS_AUTHENTICODE_CHECK_ENABLED is true
    // so the binaries should be signed. To run the test locally
    // DISABLE_UPDATER_AUTHENTICODE_CHECK can be defined.
    Assert.ok(isBinSigned,
              "the updater.exe binary should be signed (if not, build system " +
              "configuration bug?)");
  }

  // In case the machine is running an old maintenance service or if it
  // is not installed, and permissions exist to install it. Then install
  // the newer bin that we have since all of the other checks passed.
  return attemptServiceInstall();
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
  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
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
               "files! Exception: " + e);
    }
    do_timeout(TEST_CHECK_TIMEOUT, setupAppFilesAsync);
    return;
  }

  setupAppFilesFinished();
}

/**
 * Helper function for setting up the application files required to launch the
 * application for the updater tests by either copying or creating symlinks to
 * the files.
 */
function setupAppFiles() {
  debugDump("start - copying or creating symlinks to application files " +
            "for the test");

  let destDir = getApplyDirFile(null, true);
  if (!destDir.exists()) {
    try {
      destDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    } catch (e) {
      logTestInfo("unable to create directory! Path: " + destDir.path +
                  ", Exception: " + e);
      do_throw(e);
    }
  }

  // Required files for the application or the test that aren't listed in the
  // dependentlibs.list file.
  let appFiles = [ { relPath  : FILE_APP_BIN,
                     inGreDir : false },
                   { relPath  : FILE_APPLICATION_INI,
                     inGreDir : true },
                   { relPath  : "dependentlibs.list",
                     inGreDir : true } ];

  // On Linux the updater.png must also be copied
  if (IS_UNIX && !IS_MACOSX && !IS_TOOLKIT_GONK) {
    appFiles.push( { relPath  : "icons/updater.png",
                     inGreDir : true } );
  }

  // Read the dependent libs file leafnames from the dependentlibs.list file
  // into the array.
  let deplibsFile = gGREDirOrig.clone();
  deplibsFile.append("dependentlibs.list");
  let istream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  istream.init(deplibsFile, 0x01, 0o444, 0);
  istream.QueryInterface(Ci.nsILineInputStream);

  let hasMore;
  let line = {};
  do {
    hasMore = istream.readLine(line);
    appFiles.push( { relPath  : line.value,
                     inGreDir : false } );
  } while(hasMore);

  istream.close();

  appFiles.forEach(function CMAF_FLN_FE(aAppFile) {
    copyFileToTestAppDir(aAppFile.relPath, aAppFile.inGreDir);
  });

  // Copy the xpcshell updater binary
  let updater = getTestDirFile("updater.app", true);
  if (!updater.exists()) {
    updater = getTestDirFile(FILE_UPDATER_BIN);
    if (!updater.exists()) {
      do_throw("Unable to find the updater binary!");
    }
  }
  let testBinDir = getGREBinDir();
  updater.copyToFollowingLinks(testBinDir, updater.leafName);

  debugDump("finish - copying or creating symlinks to application files " +
            "for the test");
}

/**
 * Copies the specified files from the dist/bin directory into the test's
 * application directory.
 *
 * @param  aFileRelPath
 *         The relative path to the source and the destination of the file to
 *         copy.
 * @param  aInGreDir
 *         Whether the file is located in the GRE directory which is
 *         <bundle>/Contents/Resources on Mac OS X and is the installation
 *         directory on all other platforms. If false the file must be in the
 *         GRE Binary directory which is <bundle>/Contents/MacOS on Mac OS X and
 *         is the installation directory on on all other platforms.
 */
function copyFileToTestAppDir(aFileRelPath, aInGreDir) {
  // gGREDirOrig and gGREBinDirOrig must always be cloned when changing its
  // properties
  let srcFile = aInGreDir ? gGREDirOrig.clone() : gGREBinDirOrig.clone();
  let destFile = aInGreDir ? getGREDir() : getGREBinDir();
  let fileRelPath = aFileRelPath;
  let pathParts = fileRelPath.split("/");
  for (let i = 0; i < pathParts.length; i++) {
    if (pathParts[i]) {
      srcFile.append(pathParts[i]);
      destFile.append(pathParts[i]);
    }
  }

  if (IS_MACOSX && !srcFile.exists()) {
    debugDump("unable to copy file since it doesn't exist! Checking if " +
              fileRelPath + ".app exists. Path: " + srcFile.path);
    // gGREDirOrig and gGREBinDirOrig must always be cloned when changing its
    // properties
    srcFile = aInGreDir ? gGREDirOrig.clone() : gGREBinDirOrig.clone();
    destFile = aInGreDir ? getGREDir() : getGREBinDir();
    for (let i = 0; i < pathParts.length; i++) {
      if (pathParts[i]) {
        srcFile.append(pathParts[i] + (pathParts.length - 1 == i ? ".app" : ""));
        destFile.append(pathParts[i] + (pathParts.length - 1 == i ? ".app" : ""));
      }
    }
    fileRelPath = fileRelPath + ".app";
  }

  Assert.ok(srcFile.exists(),
            MSG_SHOULD_EXIST + ", leafName: " + srcFile.leafName);

  // Symlink libraries. Note that the XUL library on Mac OS X doesn't have a
  // file extension and shouldSymlink will always be false on Windows.
  let shouldSymlink = (pathParts[pathParts.length - 1] == "XUL" ||
                       fileRelPath.substr(fileRelPath.length - 3) == ".so" ||
                       fileRelPath.substr(fileRelPath.length - 6) == ".dylib");
  // The tests don't support symlinks on gonk.
  if (!shouldSymlink || IS_TOOLKIT_GONK) {
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
      let ln = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
      ln.initWithPath("/bin/ln");
      let process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
      process.init(ln);
      let args = ["-s", srcFile.path, destFile.path];
      process.run(true, args, args.length);
      Assert.ok(destFile.isSymlink(),
                "the file should be a symlink");
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
  let maintSvcDir = getMaintSvcDir();
  Assert.ok(maintSvcDir.exists(), MSG_SHOULD_EXIST);
  let oldMaintSvcBin = maintSvcDir.clone();
  oldMaintSvcBin.append(FILE_MAINTENANCE_SERVICE_BIN);
  Assert.ok(oldMaintSvcBin.exists(), MSG_SHOULD_EXIST);
  let buildMaintSvcBin = getGREBinDir();
  buildMaintSvcBin.append(FILE_MAINTENANCE_SERVICE_BIN);
  if (readFileBytes(oldMaintSvcBin) == readFileBytes(buildMaintSvcBin)) {
    debugDump("installed maintenance service binary is the same as the " +
              "build's maintenance service binary");
    return true;
  }
  let backupMaintSvcBin = maintSvcDir.clone();
  backupMaintSvcBin.append(FILE_MAINTENANCE_SERVICE_BIN + ".backup");
  try {
    if (backupMaintSvcBin.exists()) {
      backupMaintSvcBin.remove(false);
    }
    oldMaintSvcBin.moveTo(maintSvcDir, FILE_MAINTENANCE_SERVICE_BIN + ".backup");
    buildMaintSvcBin.copyTo(maintSvcDir, FILE_MAINTENANCE_SERVICE_BIN);
    backupMaintSvcBin.remove(false);
  } catch (e) {
    // Restore the original file in case the moveTo was successful.
    if (backupMaintSvcBin.exists()) {
      oldMaintSvcBin = maintSvcDir.clone();
      oldMaintSvcBin.append(FILE_MAINTENANCE_SERVICE_BIN);
      if (!oldMaintSvcBin.exists()) {
        backupMaintSvcBin.moveTo(maintSvcDir, FILE_MAINTENANCE_SERVICE_BIN);
      }
    }
    Assert.ok(false, "should be able copy the test maintenance service to " +
              "the maintenance service directory (if not, build system " +
              "configuration bug?), path: " + maintSvcDir.path);
  }

  return true;
}

/**
 * Helper function for updater tests for launching the updater using the
 * maintenance service to apply a mar file.
 *
 * @param aInitialStatus
 *        The initial value of update.status.
 * @param aExpectedStatus
 *        The expected value of update.status when the test finishes.
 * @param aCheckSvcLog (optional)
 *        Whether the service log should be checked.
 */
function runUpdateUsingService(aInitialStatus, aExpectedStatus, aCheckSvcLog) {
  // Check the service logs for a successful update
  function checkServiceLogs(aOriginalContents) {
    let contents = readServiceLogFile();
    Assert.notEqual(contents, aOriginalContents,
                    "the contents of the maintenanceservice.log should not " +
                    "be the same as the original contents");
    Assert.notEqual(contents.indexOf(LOG_SVC_SUCCESSFUL_LAUNCH), -1,
                    "the contents of the maintenanceservice.log should " +
                    "contain the successful launch string");
  }
  function readServiceLogFile() {
    let file = getMaintSvcDir();
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
    debugDump("waiting for service to stop if necessary...");
    // Use the helper bin to ensure the service is stopped. If not
    // stopped then wait for the service to be stopped (at most 120 seconds)
    let helperBin = getTestDirFile(FILE_HELPER_BIN);
    let helperBinArgs = ["wait-for-service-stop",
                         "MozillaMaintenance",
                         "120"];
    let helperBinProcess = Cc["@mozilla.org/process/util;1"].
                           createInstance(Ci.nsIProcess);
    helperBinProcess.init(helperBin);
    debugDump("stopping service...");
    helperBinProcess.run(true, helperBinArgs, helperBinArgs.length);
    Assert.notEqual(helperBinProcess.exitValue, 0xEE,
                    "the maintenance service should exist");

    if (helperBinProcess.exitValue != 0) {
      if (aFailTest) {
        Assert.ok(false, "the maintenance service should stop, process exit " +
                  "value: " + helperBinProcess.exitValue);
      }

      logTestInfo("maintenance service did not stop which may cause test " +
                  "failures later, process exit value: " +
                  helperBinProcess.exitValue);
    } else {
      debugDump("service stopped");
    }
    waitServiceApps();
  }
  function waitForApplicationStop(aApplication) {
    debugDump("waiting for " + aApplication + " to stop if necessary");
    // Use the helper bin to ensure the application is stopped.
    // If not, then wait for it to be stopped (at most 120 seconds)
    let helperBin = getTestDirFile(FILE_HELPER_BIN);
    let helperBinArgs = ["wait-for-application-exit",
                         aApplication,
                         "120"];
    let helperBinProcess = Cc["@mozilla.org/process/util;1"].
                           createInstance(Ci.nsIProcess);
    helperBinProcess.init(helperBin);
    helperBinProcess.run(true, helperBinArgs, helperBinArgs.length);
    Assert.equal(helperBinProcess.exitValue, 0,
                 "the process should have stopped, process name: " +
                 aApplication);
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
  writeStatusFile(aInitialStatus);

  // sanity check
  Assert.equal(readStatusState(), aInitialStatus,
               "the update status state" + MSG_SHOULD_EQUAL);

  writeVersionFile(DEFAULT_UPDATE_VERSION);

  gServiceLaunchedCallbackArgs = [
    "-no-remote",
    "-test-process-updates",
    "-dump-args",
    appArgsLogPath
  ];

  if (gSwitchApp) {
    // We want to set the env vars again
    gShouldResetEnv = undefined;
  }

  setEnvironment();

  let updater = getTestDirFile(FILE_UPDATER_BIN);
  if (!updater.exists()) {
    do_throw("Unable to find the updater binary!");
  }
  let testBinDir = getGREBinDir();
  updater.copyToFollowingLinks(testBinDir, updater.leafName);

  // The service will execute maintenanceservice_installer.exe and
  // will copy maintenanceservice.exe out of the same directory from
  // the installation directory.  So we need to make sure both of those
  // bins always exist in the installation directory.
  copyFileToTestAppDir(FILE_MAINTENANCE_SERVICE_BIN, false);
  copyFileToTestAppDir(FILE_MAINTENANCE_SERVICE_INSTALLER_BIN, false);

  let launchBin = getLaunchBin();
  let args = getProcessArgs(["-dump-args", appArgsLogPath]);

  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
  process.init(launchBin);
  debugDump("launching " + launchBin.path + " " + args.join(" "));
  // Firefox does not wait for the service command to finish, but
  // we still launch the process sync to avoid intermittent failures with
  // the log file not being written out yet.
  // We will rely on watching the update.status file and waiting for the service
  // to stop to know the service command is done.
  process.run(true, args, args.length);

  resetEnvironment();

  function timerCallback(aTimer) {
    // Wait for the expected status
    let status = readStatusFile();
    // status will probably always be equal to STATE_APPLYING but there is a
    // race condition where it would be possible on slower machines where status
    // could be equal to STATE_PENDING_SVC.
    if (status == STATE_APPLYING ||
        status == STATE_PENDING_SVC) {
      debugDump("still waiting to see the " + aExpectedStatus +
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
      // xpcshell tests won't display the entire contents so log each line.
      let updateLogContents = readFileBytes(updateLog).replace(/\r\n/g, "\n");
      updateLogContents = replaceLogPaths(updateLogContents);
      let aryLogContents = updateLogContents.split("\n");
      logTestInfo("contents of " + updateLog.path + ":");
      aryLogContents.forEach(function RUUS_TC_LC_FE(aLine) {
        logTestInfo(aLine);
      });
    }
    Assert.equal(status, aExpectedStatus,
                 "the update status" + MSG_SHOULD_EQUAL);

    if (aCheckSvcLog) {
      checkServiceLogs(svcOriginalLog);
    }

    checkUpdateFinished();
  }

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  timer.initWithCallback(timerCallback, 1000, timer.TYPE_REPEATING_SLACK);
}

/**
 * Gets the platform specific shell binary that is launched using nsIProcess and
 * in turn launches a binary used for the test (e.g. application, updater,
 * etc.). A shell is used so debug console output can be redirected to a file so
 * it doesn't end up in the test log.
 *
 * @return  nsIFile for the shell binary to launch using nsIProcess.
 */
function getLaunchBin() {
  let launchBin;
  if (IS_WIN) {
    launchBin = Services.dirsvc.get("WinD", Ci.nsIFile);
    launchBin.append("System32");
    launchBin.append("cmd.exe");
  } else {
    launchBin = Cc["@mozilla.org/file/local;1"].
                createInstance(Ci.nsILocalFile);
    launchBin.initWithPath("/bin/sh");
  }
  Assert.ok(launchBin.exists(), MSG_SHOULD_EXIST);

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
  let output = getApplyDirFile(DIR_RESOURCES + "output", true);
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
    debugDump("failed to remove file. Path: " + output.path);
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
  let output = getApplyDirFile(DIR_RESOURCES + "output", true);
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
    let output = getApplyDirFile(DIR_RESOURCES + "output", true);
    if (output.exists()) {
      output.remove(false);
    }
    let input = getApplyDirFile(DIR_RESOURCES + "input", true);
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
  let input = getApplyDirFile(DIR_RESOURCES + "input", true);
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
    updatesPatchDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  // Copy the mar that will be applied
  let mar = getTestDirFile(aMarFile);
  mar.copyToFollowingLinks(updatesPatchDir, FILE_UPDATE_ARCHIVE);

  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let afterApplyBinDir = getApplyDirFile(DIR_RESOURCES, true);
  helperBin.copyToFollowingLinks(afterApplyBinDir, gCallbackBinFile);
  helperBin.copyToFollowingLinks(afterApplyBinDir, gPostUpdateBinFile);

  gTestFiles.forEach(function SUT_TF_FE(aTestFile) {
    if (aTestFile.originalFile || aTestFile.originalContents) {
      let testDir = getApplyDirFile(aTestFile.relPathDir, true);
      if (!testDir.exists()) {
        testDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
      }

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

      // Skip these tests on Windows since chmod doesn't really set permissions
      // on Windows.
      if (!IS_WIN && aTestFile.originalPerms) {
        testFile.permissions = aTestFile.originalPerms;
        // Store the actual permissions on the file for reference later after
        // setting the permissions.
        if (!aTestFile.comparePerms) {
          aTestFile.comparePerms = testFile.permissions;
        }
      }
    }
  });

  // Add the test directory that will be updated for a successful update or left
  // in the initial state for a failed update.
  gTestDirs.forEach(function SUT_TD_FE(aTestDir) {
    let testDir = getApplyDirFile(aTestDir.relPathDir, true);
    if (!testDir.exists()) {
      testDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
    }

    if (aTestDir.files) {
      aTestDir.files.forEach(function SUT_TD_F_FE(aTestFile) {
        let testFile = getApplyDirFile(aTestDir.relPathDir + aTestFile, true);
        if (!testFile.exists()) {
          testFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
        }
      });
    }

    if (aTestDir.subDirs) {
      aTestDir.subDirs.forEach(function SUT_TD_SD_FE(aSubDir) {
        let testSubDir = getApplyDirFile(aTestDir.relPathDir + aSubDir, true);
        if (!testSubDir.exists()) {
          testSubDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
        }

        if (aTestDir.subDirFiles) {
          aTestDir.subDirFiles.forEach(function SUT_TD_SDF_FE(aTestFile) {
            let testFile = getApplyDirFile(aTestDir.relPathDir + aSubDir + aTestFile, true);
            if (!testFile.exists()) {
              testFile.create(Ci.nsIFile.NORMAL_FILE_TYPE, PERMS_FILE);
            }
          });
        }
      });
    }
  });
}

/**
 * Helper function for updater binary tests that creates the update-settings.ini
 * file.
 */
function createUpdateSettingsINI() {
  let updateSettingsIni = getApplyDirFile(null, true);
  if (IS_MACOSX) {
    updateSettingsIni.append("Contents");
    updateSettingsIni.append("Resources");
  }
  updateSettingsIni.append(FILE_UPDATE_SETTINGS_INI);
  writeFile(updateSettingsIni, UPDATE_SETTINGS_CONTENTS);
}

/**
 * Helper function for updater binary tests that creates the updater.ini
 * file.
 *
 * @param   aIsExeAsync
 *          True or undefined if the post update process should be async. If
 *          undefined ExeAsync will not be added to the updater.ini file in
 *          order to test the default launch behavior which is async.
 */
function createUpdaterINI(aIsExeAsync) {
  let exeArg = "ExeArg=post-update-async\n";
  let exeAsync = "";
  if (aIsExeAsync !== undefined) {
    if (aIsExeAsync) {
      exeAsync = "ExeAsync=true\n";
    } else {
      exeArg = "ExeArg=post-update-sync\n";
      exeAsync = "ExeAsync=false\n";
    }
  }

  let updaterIniContents = "[Strings]\n" +
                           "Title=Update Test\n" +
                           "Info=Running update test " + gTestID + "\n\n" +
                           "[PostUpdateMac]\n" +
                           "ExeRelPath=" + DIR_RESOURCES + gPostUpdateBinFile + "\n" +
                           exeArg +
                           exeAsync +
                           "\n" +
                           "[PostUpdateWin]\n" +
                           "ExeRelPath=" + gPostUpdateBinFile + "\n" +
                           exeArg +
                           exeAsync;
  let updaterIni = getApplyDirFile(DIR_RESOURCES + FILE_UPDATER_INI, true);
  writeFile(updaterIni, updaterIniContents);
}

/**
 * Helper function that replaces the common part of paths in the update log's
 * contents with <test_dir_path> for paths to the the test directory and
 * <update_dir_path> for paths to the update directory. This is needed since
 * Assert.equal will truncate what it prints to the xpcshell log file.
 *
 * @param   aLogContents
 *          The update log file's contents.
 * @return  the log contents with the paths replaced.
 */
function replaceLogPaths(aLogContents) {
  let logContents = aLogContents;
  // Remove the majority of the path up to the test directory. This is needed
  // since Assert.equal won't print long strings to the test logs.
  let testDirPath = do_get_file(gTestID, false).path;
  if (IS_WIN) {
    // Replace \\ with \\\\ so the regexp works.
    testDirPath = testDirPath.replace(/\\/g, "\\\\");
  }
  logContents = logContents.replace(new RegExp(testDirPath, "g"),
                                    "<test_dir_path>/" + gTestID);
  let updatesDirPath = getMockUpdRootD().path;
  if (IS_WIN) {
    // Replace \\ with \\\\ so the regexp works.
    updatesDirPath = updatesDirPath.replace(/\\/g, "\\\\");
  }
  logContents = logContents.replace(new RegExp(updatesDirPath, "g"),
                                    "<update_dir_path>/" + gTestID);
  if (IS_WIN) {
    // Replace \ with /
    logContents = logContents.replace(/\\/g, "/");
  }
  return logContents;
}

/**
 * Helper function for updater binary tests for verifying the contents of the
 * update log after a successful update.
 *
 * @param   aCompareLogFile
 *          The log file to compare the update log with.
 * @param   aExcludeDistributionDir
 *          Removes lines containing the distribution directory from the log
 *          file to compare the update log with.
 */
function checkUpdateLogContents(aCompareLogFile, aExcludeDistributionDir) {
  if (IS_UNIX && !IS_MACOSX) {
    // Sorting on Linux is different so skip checking the logs for now.
    return;
  }

  let updateLog = getUpdatesPatchDir();
  updateLog.append(FILE_UPDATE_LOG);
  let updateLogContents = readFileBytes(updateLog);

  // The channel-prefs.js is defined in gTestFilesCommon which will always be
  // located to the end of gTestFiles when it is present.
  if (gTestFiles.length > 1 &&
      gTestFiles[gTestFiles.length - 1].fileName == "channel-prefs.js" &&
      !gTestFiles[gTestFiles.length - 1].originalContents) {
    updateLogContents = updateLogContents.replace(/.*defaults\/.*/g, "");
  }
  if (gTestFiles.length > 2 &&
      gTestFiles[gTestFiles.length - 2].fileName == FILE_UPDATE_SETTINGS_INI &&
      !gTestFiles[gTestFiles.length - 2].originalContents) {
    updateLogContents = updateLogContents.replace(/.*update-settings.ini.*/g, "");
  }
  if (gStageUpdate) {
    // Skip the staged update messages
    updateLogContents = updateLogContents.replace(/Performing a staged update/, "");
  } else if (gSwitchApp) {
    // Skip the switch app request messages
    updateLogContents = updateLogContents.replace(/Performing a staged update/, "");
    updateLogContents = updateLogContents.replace(/Performing a replace request/, "");
  }
  // Skip the source/destination lines since they contain absolute paths.
  updateLogContents = updateLogContents.replace(/PATCH DIRECTORY.*/g, "");
  updateLogContents = updateLogContents.replace(/INSTALLATION DIRECTORY.*/g, "");
  updateLogContents = updateLogContents.replace(/WORKING DIRECTORY.*/g, "");
  // Skip lines that log failed attempts to open the callback executable.
  updateLogContents = updateLogContents.replace(/NS_main: callback app file .*/g, "");
  if (IS_MACOSX) {
    // Skip lines that log moving the distribution directory for Mac v2 signing.
    updateLogContents = updateLogContents.replace(/Moving old [^\n]*\nrename_file: .*/g, "");
    updateLogContents = updateLogContents.replace(/New distribution directory .*/g, "");
  }
  if (gSwitchApp) {
    // Remove the lines which contain absolute paths
    updateLogContents = updateLogContents.replace(/^Begin moving.*$/mg, "");
    updateLogContents = updateLogContents.replace(/^ensure_remove: failed to remove file: .*$/mg, "");
    updateLogContents = updateLogContents.replace(/^ensure_remove_recursive: unable to remove directory: .*$/mg, "");
    updateLogContents = updateLogContents.replace(/^Removing tmpDir failed, err: -1$/mg, "");
    updateLogContents = updateLogContents.replace(/^remove_recursive_on_reboot: .*$/mg, "");
  }
  updateLogContents = updateLogContents.replace(/\r/g, "");
  // Replace error codes since they are different on each platform.
  updateLogContents = updateLogContents.replace(/, err:.*\n/g, "\n");
  // Replace to make the log parsing happy.
  updateLogContents = updateLogContents.replace(/non-fatal error /g, "");
  // The FindFile results when enumerating the filesystem on Windows is not
  // determistic so the results matching the following need to be ignored.
  updateLogContents = updateLogContents.replace(/.*7\/7text.*\n/g, "");
  // Remove consecutive newlines
  updateLogContents = updateLogContents.replace(/\n+/g, "\n");
  // Remove leading and trailing newlines
  updateLogContents = updateLogContents.replace(/^\n|\n$/g, "");
  updateLogContents = replaceLogPaths(updateLogContents);

  let compareLogContents = "";
  if (aCompareLogFile) {
    compareLogContents = readFileBytes(getTestDirFile(aCompareLogFile));
  }
  if (gSwitchApp) {
    compareLogContents += LOG_SWITCH_SUCCESS;
  }
  // The channel-prefs.js is defined in gTestFilesCommon which will always be
  // located to the end of gTestFiles.
  if (gTestFiles.length > 1 &&
      gTestFiles[gTestFiles.length - 1].fileName == "channel-prefs.js" &&
      !gTestFiles[gTestFiles.length - 1].originalContents) {
    compareLogContents = compareLogContents.replace(/.*defaults\/.*/g, "");
  }
  if (gTestFiles.length > 2 &&
      gTestFiles[gTestFiles.length - 2].fileName == FILE_UPDATE_SETTINGS_INI &&
      !gTestFiles[gTestFiles.length - 2].originalContents) {
    compareLogContents = compareLogContents.replace(/.*update-settings.ini.*/g, "");
  }
  if (aExcludeDistributionDir) {
    compareLogContents = compareLogContents.replace(/.*distribution\/.*/g, "");
  }
  // Remove leading and trailing newlines
  compareLogContents = compareLogContents.replace(/\n+/g, "\n");
  // Remove leading and trailing newlines
  compareLogContents = compareLogContents.replace(/^\n|\n$/g, "");

  // Don't write the contents of the file to the log to reduce log spam
  // unless there is a failure.
  if (compareLogContents == updateLogContents) {
    Assert.ok(true, "the update log contents" + MSG_SHOULD_EQUAL);
  } else {
    logTestInfo("the update log contents are not correct");
    let aryLog = updateLogContents.split("\n");
    let aryCompare = compareLogContents.split("\n");
    // Pushing an empty string to both arrays makes it so either array's length
    // can be used in the for loop below without going out of bounds.
    aryLog.push("");
    aryCompare.push("");
    // xpcshell tests won't display the entire contents so log the incorrect
    // line.
    for (let i = 0; i < aryLog.length; ++i) {
      if (aryLog[i] != aryCompare[i]) {
        logTestInfo("the first incorrect line in the update log is: " +
                    aryLog[i]);
        Assert.equal(aryLog[i], aryCompare[i],
                     "the update log contents" + MSG_SHOULD_EQUAL);
      }
    }
    // This should never happen!
    do_throw("Unable to find incorrect update log contents!");
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
  updateLogContents = replaceLogPaths(updateLogContents);
  Assert.notEqual(updateLogContents.indexOf(aCheckString), -1,
                  "the update log contents should contain value: " +
                  aCheckString);
}

/**
 * Helper function for updater binary tests for verifying the state of files and
 * directories after a successful update.
 *
 * @param  aGetFileFunc
 *         The function used to get the files in the directory to be checked.
 * @param  aStageDirExists
 *         If true the staging directory will be tested for existence and if
 *         false the staging directory will be tested for non-existence.
 * @param  aToBeDeletedDirExists
 *         On Windows, if true the tobedeleted directory will be tested for
 *         existence and if false the tobedeleted directory will be tested for
 *         non-existence. On all othere platforms it will be tested for
 *         non-existence.
 */
function checkFilesAfterUpdateSuccess(aGetFileFunc, aStageDirExists,
                                      aToBeDeletedDirExists) {
  debugDump("testing contents of files after a successful update");
  gTestFiles.forEach(function CFAUS_TF_FE(aTestFile) {
    let testFile = aGetFileFunc(aTestFile.relPathDir + aTestFile.fileName, true);
    debugDump("testing file: " + testFile.path);
    if (aTestFile.compareFile || aTestFile.compareContents) {
      Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);

      // Skip these tests on Windows since chmod doesn't really set permissions
      // on Windows.
      if (!IS_WIN && aTestFile.comparePerms) {
        // Check if the permssions as set in the complete mar file are correct.
        Assert.equal(testFile.permissions & 0xfff,
                     aTestFile.comparePerms & 0xfff,
                     "the file permissions" + MSG_SHOULD_EQUAL);
      }

      let fileContents1 = readFileBytes(testFile);
      let fileContents2 = aTestFile.compareFile ?
                          readFileBytes(getTestDirFile(aTestFile.compareFile)) :
                          aTestFile.compareContents;
      // Don't write the contents of the file to the log to reduce log spam
      // unless there is a failure.
      if (fileContents1 == fileContents2) {
        Assert.ok(true, "the file contents" + MSG_SHOULD_EQUAL);
      } else {
        Assert.equal(fileContents1, fileContents2,
                     "the file contents" + MSG_SHOULD_EQUAL);
      }
    } else {
      Assert.ok(!testFile.exists(), MSG_SHOULD_NOT_EXIST);
    }
  });

  debugDump("testing operations specified in removed-files were performed " +
            "after a successful update");
  gTestDirs.forEach(function CFAUS_TD_FE(aTestDir) {
    let testDir = aGetFileFunc(aTestDir.relPathDir, true);
    debugDump("testing directory: " + testDir.path);
    if (aTestDir.dirRemoved) {
      Assert.ok(!testDir.exists(), MSG_SHOULD_NOT_EXIST);
    } else {
      Assert.ok(testDir.exists(), MSG_SHOULD_EXIST);

      if (aTestDir.files) {
        aTestDir.files.forEach(function CFAUS_TD_F_FE(aTestFile) {
          let testFile = aGetFileFunc(aTestDir.relPathDir + aTestFile, true);
          if (aTestDir.filesRemoved) {
            Assert.ok(!testFile.exists(), MSG_SHOULD_NOT_EXIST);
          } else {
            Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);
          }
        });
      }

      if (aTestDir.subDirs) {
        aTestDir.subDirs.forEach(function CFAUS_TD_SD_FE(aSubDir) {
          let testSubDir = aGetFileFunc(aTestDir.relPathDir + aSubDir, true);
          Assert.ok(testSubDir.exists(), MSG_SHOULD_EXIST);
          if (aTestDir.subDirFiles) {
            aTestDir.subDirFiles.forEach(function CFAUS_TD_SDF_FE(aTestFile) {
              let testFile = aGetFileFunc(aTestDir.relPathDir +
                                          aSubDir + aTestFile, true);
              Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);
            });
          }
        });
      }
    }
  });

  checkFilesAfterUpdateCommon(aGetFileFunc, aStageDirExists,
                              aToBeDeletedDirExists);
}

/**
 * Helper function for updater binary tests for verifying the state of files and
 * directories after a failed update.
 *
 * @param aGetFileFunc
 *        the function used to get the files in the directory to be checked.
 * @param  aStageDirExists
 *         If true the staging directory will be tested for existence and if
 *         false the staging directory will be tested for non-existence.
 * @param  aToBeDeletedDirExists
 *         On Windows, if true the tobedeleted directory will be tested for
 *         existence and if false the tobedeleted directory will be tested for
 *         non-existence. On all othere platforms it will be tested for
 *         non-existence.
 */
function checkFilesAfterUpdateFailure(aGetFileFunc, aStageDirExists,
                                      aToBeDeletedDirExists) {
  debugDump("testing contents of files after a failed update");
  gTestFiles.forEach(function CFAUF_TF_FE(aTestFile) {
    let testFile = aGetFileFunc(aTestFile.relPathDir + aTestFile.fileName, true);
    debugDump("testing file: " + testFile.path);
    if (aTestFile.compareFile || aTestFile.compareContents) {
      Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);

      // Skip these tests on Windows since chmod doesn't really set permissions
      // on Windows.
      if (!IS_WIN && aTestFile.comparePerms) {
        // Check the original permssions are retained on the file.
        Assert.equal(testFile.permissions & 0xfff,
                     aTestFile.comparePerms & 0xfff,
                     "the file permissions" + MSG_SHOULD_EQUAL);
      }

      let fileContents1 = readFileBytes(testFile);
      let fileContents2 = aTestFile.compareFile ?
                          readFileBytes(getTestDirFile(aTestFile.compareFile)) :
                          aTestFile.compareContents;
      // Don't write the contents of the file to the log to reduce log spam
      // unless there is a failure.
      if (fileContents1 == fileContents2) {
        Assert.ok(true, "the file contents" + MSG_SHOULD_EQUAL);
      } else {
        Assert.equal(fileContents1, fileContents2,
                     "the file contents" + MSG_SHOULD_EQUAL);
      }
    } else {
      Assert.ok(!testFile.exists(), MSG_SHOULD_NOT_EXIST);
    }
  });

  debugDump("testing operations specified in removed-files were not " +
            "performed after a failed update");
  gTestDirs.forEach(function CFAUF_TD_FE(aTestDir) {
    let testDir = aGetFileFunc(aTestDir.relPathDir, true);
    Assert.ok(testDir.exists(), MSG_SHOULD_EXIST);

    if (aTestDir.files) {
      aTestDir.files.forEach(function CFAUS_TD_F_FE(aTestFile) {
        let testFile = aGetFileFunc(aTestDir.relPathDir + aTestFile, true);
        Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);
      });
    }

    if (aTestDir.subDirs) {
      aTestDir.subDirs.forEach(function CFAUS_TD_SD_FE(aSubDir) {
        let testSubDir = aGetFileFunc(aTestDir.relPathDir + aSubDir, true);
        Assert.ok(testSubDir.exists(), MSG_SHOULD_EXIST);
        if (aTestDir.subDirFiles) {
          aTestDir.subDirFiles.forEach(function CFAUS_TD_SDF_FE(aTestFile) {
            let testFile = aGetFileFunc(aTestDir.relPathDir +
                                        aSubDir + aTestFile, true);
            Assert.ok(testFile.exists(), MSG_SHOULD_EXIST);
          });
        }
      });
    }
  });

  checkFilesAfterUpdateCommon(aGetFileFunc, aStageDirExists,
                              aToBeDeletedDirExists);
}

/**
 * Helper function for updater binary tests for verifying the state of common
 * files and directories after a successful or failed update.
 *
 * @param aGetFileFunc
 *        the function used to get the files in the directory to be checked.
 * @param  aStageDirExists
 *         If true the staging directory will be tested for existence and if
 *         false the staging directory will be tested for non-existence.
 * @param  aToBeDeletedDirExists
 *         On Windows, if true the tobedeleted directory will be tested for
 *         existence and if false the tobedeleted directory will be tested for
 *         non-existence. On all othere platforms it will be tested for
 *         non-existence.
 */
function checkFilesAfterUpdateCommon(aGetFileFunc, aStageDirExists,
                                     aToBeDeletedDirExists) {
  debugDump("testing extra directories");
  let stageDir = getStageDirFile(null, true);
  if (aStageDirExists) {
    Assert.ok(stageDir.exists(), MSG_SHOULD_EXIST);
  } else {
    Assert.ok(!stageDir.exists(), MSG_SHOULD_NOT_EXIST);
  }

  let toBeDeletedDirExists = IS_WIN ? aToBeDeletedDirExists : false;
  let toBeDeletedDir = getApplyDirFile(DIR_TOBEDELETED, true);
  if (toBeDeletedDirExists) {
    Assert.ok(toBeDeletedDir.exists(), MSG_SHOULD_EXIST);
  } else {
    Assert.ok(!toBeDeletedDir.exists(), MSG_SHOULD_NOT_EXIST);
  }

  let updatingDir = getApplyDirFile("updating", true);
  Assert.ok(!updatingDir.exists(), MSG_SHOULD_NOT_EXIST);

  if (stageDir.exists()) {
    updatingDir = stageDir.clone();
    updatingDir.append("updating");
    Assert.ok(!updatingDir.exists(), MSG_SHOULD_NOT_EXIST);
  }

  debugDump("testing backup files should not be left behind in the " +
            "application directory");
  let applyToDir = getApplyDirFile(null, true);
  checkFilesInDirRecursive(applyToDir, checkForBackupFiles);

  if (stageDir.exists()) {
    debugDump("testing backup files should not be left behind in the " +
              "staging directory");
    let applyToDir = getApplyDirFile(null, true);
    checkFilesInDirRecursive(stageDir, checkForBackupFiles);
  }

  debugDump("testing patch files should not be left behind");
  let updatesDir = getUpdatesPatchDir();
  let entries = updatesDir.QueryInterface(Ci.nsIFile).directoryEntries;
  while (entries.hasMoreElements()) {
    let entry = entries.getNext().QueryInterface(Ci.nsIFile);
    Assert.notEqual(getFileExtension(entry), "patch",
                    "the file's extension should not equal patch");
  }
}

/**
 * Helper function for updater binary tests for verifying the contents of the
 * updater callback application log which should contain the arguments passed to
 * the callback application.
 */
function checkCallbackAppLog() {
  let appLaunchLog = getApplyDirFile(DIR_RESOURCES + gCallbackArgs[1], true);
  if (!appLaunchLog.exists()) {
    do_timeout(TEST_HELPER_TIMEOUT, checkCallbackAppLog);
    return;
  }

  let expectedLogContents = gCallbackArgs.join("\n") + "\n";
  let logContents = readFile(appLaunchLog);
  // It is possible for the log file contents check to occur before the log file
  // contents are completely written so wait until the contents are the expected
  // value. If the contents are never the expected value then the test will
  // fail by timing out after gTimeoutRuns is greater than MAX_TIMEOUT_RUNS or
  // the test harness times out the test.
  if (logContents != expectedLogContents) {
    gTimeoutRuns++;
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      logTestInfo("callback log contents are not correct");
      // This file doesn't contain full paths so there is no need to call
      // replaceLogPaths.
      let aryLog = logContents.split("\n");
      let aryCompare = expectedLogContents.split("\n");
      // Pushing an empty string to both arrays makes it so either array's length
      // can be used in the for loop below without going out of bounds.
      aryLog.push("");
      aryCompare.push("");
      // xpcshell tests won't display the entire contents so log the incorrect
      // line.
      for (let i = 0; i < aryLog.length; ++i) {
        if (aryLog[i] != aryCompare[i]) {
          logTestInfo("the first incorrect line in the callback log is: " +
                      aryLog[i]);
          Assert.equal(aryLog[i], aryCompare[i],
                       "the callback log contents" + MSG_SHOULD_EQUAL);
        }
      }
      // This should never happen!
      do_throw("Unable to find incorrect callback log contents!");
    }
    do_timeout(TEST_HELPER_TIMEOUT, checkCallbackAppLog);
    return;
  }
  Assert.ok(true, "the callback log contents" + MSG_SHOULD_EQUAL);

  waitForFilesInUse();
}

/**
 * Helper function for updater binary tests for getting the log and running
 * files created by the test helper binary file when called with the post-update
 * command line argument.
 *
 * @param   aSuffix
 *          The string to append to the post update test helper binary path.
 */
function getPostUpdateFile(aSuffix) {
  return getApplyDirFile(DIR_RESOURCES + gPostUpdateBinFile + aSuffix, true);
}

/**
 * Helper function for updater binary tests for verifying the contents of the
 * updater post update binary log.
 */
function checkPostUpdateAppLog() {
  gTimeoutRuns++;
  let postUpdateLog = getPostUpdateFile(".log");
  if (!postUpdateLog.exists()) {
    debugDump("postUpdateLog does not exist. Path: " + postUpdateLog.path);
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the post update " +
               "process to create the post update log. Path: " +
               postUpdateLog.path);
    }
    do_timeout(TEST_HELPER_TIMEOUT, checkPostUpdateAppLog);
    return;
  }

  let logContents = readFile(postUpdateLog);
  // It is possible for the log file contents check to occur before the log file
  // contents are completely written so wait until the contents are the expected
  // value. If the contents are never the expected value then the test will
  // fail by timing out after gTimeoutRuns is greater than MAX_TIMEOUT_RUNS or
  // the test harness times out the test.
  if (logContents != "post-update\n") {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the post update " +
               "process to create the expected contents in the post update log. Path: " +
               postUpdateLog.path);
    }
    do_timeout(TEST_HELPER_TIMEOUT, checkPostUpdateAppLog);
    return;
  }
  Assert.ok(true, "the post update log contents" + MSG_SHOULD_EQUAL);

  gCheckFunc();
}

/**
 * Helper function for updater service tests for verifying the contents of the
 * updater callback application log which should contain the arguments passed to
 * the callback application.
 */
function checkCallbackServiceLog() {
  Assert.ok(!!gServiceLaunchedCallbackLog,
            "gServiceLaunchedCallbackLog should be defined");

  let expectedLogContents = gServiceLaunchedCallbackArgs.join("\n") + "\n";
  let logFile = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  logFile.initWithPath(gServiceLaunchedCallbackLog);
  let logContents = readFile(logFile);
  // It is possible for the log file contents check to occur before the log file
  // contents are completely written so wait until the contents are the expected
  // value. If the contents are never the expected value then the test will
  // fail by timing out after gTimeoutRuns is greater than MAX_TIMEOUT_RUNS or
  // the test harness times out the test.
  if (logContents != expectedLogContents) {
    gTimeoutRuns++;
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      logTestInfo("callback service log contents are not correct");
      let aryLog = logContents.split("\n");
      let aryCompare = expectedLogContents.split("\n");
      // Pushing an empty string to both arrays makes it so either array's length
      // can be used in the for loop below without going out of bounds.
      aryLog.push("");
      aryCompare.push("");
      // xpcshell tests won't display the entire contents so log the incorrect
      // line.
      for (let i = 0; i < aryLog.length; ++i) {
        if (aryLog[i] != aryCompare[i]) {
          logTestInfo("the first incorrect line in the service callback log " +
                      "is: " + aryLog[i]);
          Assert.equal(aryLog[i], aryCompare[i],
                       "the service callback log contents" + MSG_SHOULD_EQUAL);
        }
      }
      // This should never happen!
      do_throw("Unable to find incorrect service callback log contents!");
    }
    do_timeout(TEST_HELPER_TIMEOUT, checkCallbackServiceLog);
    return;
  }
  Assert.ok(true, "the callback service log contents" + MSG_SHOULD_EQUAL);

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

    for (let i = 0; i < files.length; ++i) {
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
          debugDump("file is not in use. Path: " + file.path);
        } catch (e) {
          debugDump("file in use, will try again after " + TEST_CHECK_TIMEOUT +
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

  debugDump("calling doTestFinish");
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
  Assert.notEqual(getFileExtension(aFile), "moz-backup",
                  "the file's extension should not equal moz-backup");
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
  if (!aDir.exists()) {
    do_throw("Directory must exist!");
  }

  let dirEntries = aDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    let entry = dirEntries.getNext().QueryInterface(Ci.nsIFile);

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
 *            function callHandleEvent(aXHR) {
 *              aXHR.status = gExpectedStatus;
 *              let e = { target: aXHR };
 *              aXHR.onload.handleEvent(e);
 *            }
 */
function overrideXHR(aCallback) {
  Cu.import("resource://testing-common/MockRegistrar.jsm");
  MockRegistrar.register("@mozilla.org/xmlextras/xmlhttprequest;1", xhr, [aCallback]);
}


/**
 * Bare bones XMLHttpRequest implementation for testing onprogress, onerror,
 * and onload nsIDomEventListener handleEvent.
 */
function makeHandler(aVal) {
  if (typeof aVal == "function") {
    return { handleEvent: aVal };
  }
  return aVal;
}
function xhr(aCallback) {
  this._callback = aCallback;
}
xhr.prototype = {
  overrideMimeType: function(aMimetype) { },
  setRequestHeader: function(aHeader, aValue) { },
  status: null,
  channel: {
    set notificationCallbacks(aVal) { },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel])
  },
  _url: null,
  _method: null,
  open: function(aMethod, aUrl) {
    this.channel.originalURI = Services.io.newURI(aUrl, null, null);
    this._method = aMethod; this._url = aUrl;
  },
  responseXML: null,
  responseText: null,
  send: function(aBody) {
    do_execute_soon(function() {
      this._callback(this);
    }.bind(this)); // Use a timeout so the XHR completes
  },
  _onprogress: null,
  set onprogress(aValue) { this._onprogress = makeHandler(aValue); },
  get onprogress() { return this._onprogress; },
  _onerror: null,
  set onerror(aValue) { this._onerror = makeHandler(aValue); },
  get onerror() { return this._onerror; },
  _onload: null,
  set onload(aValue) { this._onload = makeHandler(aValue); },
  get onload() { return this._onload; },
  addEventListener: function(aEvent, aValue, aCapturing) {
    eval("this._on" + aEvent + " = aValue");
  },
  flags: Ci.nsIClassInfo.SINGLETON,
  getScriptableHelper: () => null,
  getInterfaces: function(aCount) {
    let interfaces = [Ci.nsISupports];
    aCount.value = interfaces.length;
    return interfaces;
  },
  get wrappedJSObject() { return this; },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIClassInfo])
};

/**
 * Helper function to override the update prompt component to verify whether it
 * is called or not.
 *
 * @param   aCallback
 *          The callback to call if the update prompt component is called.
 */
function overrideUpdatePrompt(aCallback) {
  Cu.import("resource://testing-common/MockRegistrar.jsm");
  MockRegistrar.register("@mozilla.org/updates/update-prompt;1", UpdatePrompt, [aCallback]);
}

function UpdatePrompt(aCallback) {
  this._callback = aCallback;

  let fns = ["checkForUpdates", "showUpdateAvailable", "showUpdateDownloaded",
             "showUpdateError", "showUpdateHistory", "showUpdateInstalled"];

  fns.forEach(function UP_fns(aPromptFn) {
    UpdatePrompt.prototype[aPromptFn] = function() {
      if (!this._callback) {
        return;
      }

      let callback = this._callback[aPromptFn];
      if (!callback) {
        return;
      }

      callback.apply(this._callback,
                     Array.prototype.slice.call(arguments));
    }
  });
}

UpdatePrompt.prototype = {
  flags: Ci.nsIClassInfo.SINGLETON,
  getScriptableHelper: () => null,
  getInterfaces: function(aCount) {
    let interfaces = [Ci.nsISupports, Ci.nsIUpdatePrompt];
    aCount.value = interfaces.length;
    return interfaces;
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIClassInfo, Ci.nsIUpdatePrompt])
};

/* Update check listener */
const updateCheckListener = {
  onProgress: function UCL_onProgress(aRequest, aPosition, aTotalSize) {
  },

  onCheckComplete: function UCL_onCheckComplete(aRequest, aUpdates, aUpdateCount) {
    gRequestURL = aRequest.channel.originalURI.spec;
    gUpdateCount = aUpdateCount;
    gUpdates = aUpdates;
    debugDump("url = " + gRequestURL + ", " +
              "request.status = " + aRequest.status + ", " +
              "updateCount = " + aUpdateCount);
    // Use a timeout to allow the XHR to complete
    do_execute_soon(gCheckFunc);
  },

  onError: function UCL_onError(aRequest, aUpdate) {
    gRequestURL = aRequest.channel.originalURI.spec;
    gStatusCode = aRequest.status;
    gStatusText = aUpdate.statusText ? aUpdate.statusText : null;
    debugDump("url = " + gRequestURL + ", " +
              "request.status = " + gStatusCode + ", " +
              "update.statusText = " + gStatusText);
    // Use a timeout to allow the XHR to complete
    do_execute_soon(gCheckFunc.bind(null, aRequest, aUpdate));
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateCheckListener])
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

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIProgressEventSink])
};

/**
 * Helper for starting the http server used by the tests
 */
function start_httpserver() {
  let dir = getTestDirFile();
  debugDump("http server directory path: " + dir.path);

  if (!dir.isDirectory()) {
    do_throw("A file instead of a directory was specified for HttpServer " +
             "registerDirectory! Path: " + dir.path);
  }

  let { HttpServer } = Cu.import("resource://testing-common/httpd.js", {});
  gTestserver = new HttpServer();
  gTestserver.registerDirectory("/", dir);
  gTestserver.start(-1);
  let testserverPort = gTestserver.identity.primaryPort;
  gURLData = URL_HOST + ":" + testserverPort + "/";
  debugDump("http server port = " + testserverPort);
}

/**
 * Helper for stopping the http server used by the tests
 *
 * @param   aCallback
 *          The callback to call after stopping the http server.
 */
function stop_httpserver(aCallback) {
  Assert.ok(!!aCallback, "the aCallback parameter should be defined");
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
  let ifaces = [Ci.nsIXULAppInfo, Ci.nsIXULRuntime];
  if (IS_WIN) {
    ifaces.push(Ci.nsIWinAppHelper);
  }
  const XULAppInfo = {
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

    QueryInterface: XPCOMUtils.generateQI(ifaces)
  };

  const XULAppInfoFactory = {
    createInstance: function(aOuter, aIID) {
      if (aOuter == null) {
        return XULAppInfo.QueryInterface(aIID);
      }
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
  };

  let registrar = Cm.QueryInterface(Ci.nsIComponentRegistrar);
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
 * -test-process-updates makes the application exit after being relaunched by
 * the updater.
 * the platform specific string defined by PIPE_TO_NULL to output both stdout
 * and stderr to null. This is needed to prevent output from the application
 * from ending up in the xpchsell log.
 */
function getProcessArgs(aExtraArgs) {
  if (!aExtraArgs) {
    aExtraArgs = [];
  }

  let appBinPath = getApplyDirFile(DIR_MACOS + FILE_APP_BIN, false).path;
  if (/ /.test(appBinPath)) {
    appBinPath = '"' + appBinPath + '"';
  }

  let args;
  if (IS_UNIX) {
    let launchScript = getLaunchScript();
    // Precreate the script with executable permissions
    launchScript.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, PERMS_DIRECTORY);

    let scriptContents = "#! /bin/sh\n";
    scriptContents += appBinPath + " -no-remote -test-process-updates " +
                      aExtraArgs.join(" ") + " " + PIPE_TO_NULL;
    writeFile(launchScript, scriptContents);
    debugDump("created " + launchScript.path + " containing:\n" +
              scriptContents);
    args = [launchScript.path];
  } else {
    args = ["/D", "/Q", "/C", appBinPath, "-no-remote", "-test-process-updates"].
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
            return getApplyDirFile(DIR_RESOURCES, true);
          }
          break;
        case NS_GRE_BIN_DIR:
          if (gUseTestAppDir) {
            return getApplyDirFile(DIR_MACOS, true);
          }
          break;
        case XRE_EXECUTABLE_FILE:
          if (gUseTestAppDir) {
            return getApplyDirFile(DIR_MACOS + FILE_APP_BIN, true);
          }
          break;
        case XRE_UPDATE_ROOT_DIR:
          return getMockUpdRootD();
      }
      return null;
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider])
  };
  let ds = Services.dirsvc.QueryInterface(Ci.nsIDirectoryService);
  ds.QueryInterface(Ci.nsIProperties).undefine(NS_GRE_DIR);
  ds.QueryInterface(Ci.nsIProperties).undefine(NS_GRE_BIN_DIR);
  ds.QueryInterface(Ci.nsIProperties).undefine(XRE_EXECUTABLE_FILE);
  ds.registerProvider(dirProvider);
  do_register_cleanup(function AGP_cleanup() {
    debugDump("start - unregistering directory provider");

    if (gAppTimer) {
      debugDump("start - cancel app timer");
      gAppTimer.cancel();
      gAppTimer = null;
      debugDump("finish - cancel app timer");
    }

    if (gProcess && gProcess.isRunning) {
      debugDump("start - kill process");
      try {
        gProcess.kill();
      } catch (e) {
        debugDump("kill process failed. Exception: " + e);
      }
      gProcess = null;
      debugDump("finish - kill process");
    }

    if (gHandle) {
      try {
        debugDump("start - closing handle");
        let kernel32 = ctypes.open("kernel32");
        let CloseHandle = kernel32.declare("CloseHandle", ctypes.default_abi,
                                           ctypes.bool, /*return*/
                                           ctypes.voidptr_t /*handle*/);
        if (!CloseHandle(gHandle)) {
          debugDump("call to CloseHandle failed");
        }
        kernel32.close();
        gHandle = null;
        debugDump("finish - closing handle");
      } catch (e) {
        debugDump("call to CloseHandle failed. Exception: " + e);
      }
    }

    // Call end_test first before the directory provider is unregistered
    if (typeof(end_test) == typeof(Function)) {
      debugDump("calling end_test");
      end_test();
    }

    ds.unregisterProvider(dirProvider);
    cleanupTestCommon();

    debugDump("finish - unregistering directory provider");
  });
}


/**
 * Helper function for launching the application to apply an update.
 */
function launchAppToApplyUpdate() {
  debugDump("start - launching application to apply update");

  let appBin = getApplyDirFile(DIR_MACOS + FILE_APP_BIN, false);

  if (typeof(customLaunchAppToApplyUpdate) == typeof(Function)) {
    customLaunchAppToApplyUpdate();
  }

  let launchBin = getLaunchBin();
  let args = getProcessArgs();
  debugDump("launching " + launchBin.path + " " + args.join(" "));

  gProcess = Cc["@mozilla.org/process/util;1"].
             createInstance(Ci.nsIProcess);
  gProcess.init(launchBin);

  gAppTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  gAppTimer.initWithCallback(gTimerCallback, APP_TIMER_TIMEOUT,
                             Ci.nsITimer.TYPE_ONE_SHOT);

  setEnvironment();
  debugDump("launching application");
  gProcess.runAsync(args, args.length, gProcessObserver);
  resetEnvironment();

  debugDump("finish - launching application to apply update");
}

/**
 * The observer for the call to nsIProcess:runAsync.
 */
const gProcessObserver = {
  observe: function PO_observe(aSubject, aTopic, aData) {
    debugDump("topic: " + aTopic + ", process exitValue: " +
              gProcess.exitValue);
    if (gAppTimer) {
      gAppTimer.cancel();
      gAppTimer = null;
    }
    Assert.equal(gProcess.exitValue, 0,
                 "the application process exit value should be 0");
    Assert.equal(aTopic, "process-finished",
                 "the application process observer topic should be " +
                 "process-finished");
    do_timeout(TEST_CHECK_TIMEOUT, checkUpdateFinished);
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

/**
 * The timer callback to kill the process if it takes too long.
 */
const gTimerCallback = {
  notify: function TC_notify(aTimer) {
    gAppTimer = null;
    if (gProcess.isRunning) {
      logTestInfo("attempting to kill process");
      gProcess.kill();
    }
    Assert.ok(false, "launch application timer expired");
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback])
};

/**
 * The update-staged observer for the call to nsIUpdateProcessor:processUpdate.
 */
const gUpdateStagedObserver = {
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "update-staged") {
      Services.obs.removeObserver(gUpdateStagedObserver, "update-staged");
      checkUpdateApplied();
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

/**
 * Sets the environment that will be used by the application process when it is
 * launched.
 */
function setEnvironment() {
  // Prevent setting the environment more than once.
  if (gShouldResetEnv !== undefined) {
    return;
  }

  gShouldResetEnv = true;

  let env = Cc["@mozilla.org/process/environment;1"].
            getService(Ci.nsIEnvironment);
  if (IS_WIN && !env.exists("XRE_NO_WINDOWS_CRASH_DIALOG")) {
    gAddedEnvXRENoWindowsCrashDialog = true;
    debugDump("setting the XRE_NO_WINDOWS_CRASH_DIALOG environment " +
              "variable to 1... previously it didn't exist");
    env.set("XRE_NO_WINDOWS_CRASH_DIALOG", "1");
  }

  if (IS_UNIX) {
    let appGreBinDir = gGREBinDirOrig.clone();
    let envGreBinDir = Cc["@mozilla.org/file/local;1"].
                       createInstance(Ci.nsILocalFile);
    let shouldSetEnv = true;
    if (IS_MACOSX) {
      if (env.exists("DYLD_LIBRARY_PATH")) {
        gEnvDyldLibraryPath = env.get("DYLD_LIBRARY_PATH");
        envGreBinDir.initWithPath(gEnvDyldLibraryPath);
        if (envGreBinDir.path == appGreBinDir.path) {
          gEnvDyldLibraryPath = null;
          shouldSetEnv = false;
        }
      }

      if (shouldSetEnv) {
        debugDump("setting DYLD_LIBRARY_PATH environment variable value to " +
                  appGreBinDir.path);
        env.set("DYLD_LIBRARY_PATH", appGreBinDir.path);
      }
    } else {
      if (env.exists("LD_LIBRARY_PATH")) {
        gEnvLdLibraryPath = env.get("LD_LIBRARY_PATH");
        envGreBinDir.initWithPath(gEnvLdLibraryPath);
        if (envGreBinDir.path == appGreBinDir.path) {
          gEnvLdLibraryPath = null;
          shouldSetEnv = false;
        }
      }

      if (shouldSetEnv) {
        debugDump("setting LD_LIBRARY_PATH environment variable value to " +
                  appGreBinDir.path);
        env.set("LD_LIBRARY_PATH", appGreBinDir.path);
      }
    }
  }

  if (env.exists("XPCOM_MEM_LEAK_LOG")) {
    gEnvXPCOMMemLeakLog = env.get("XPCOM_MEM_LEAK_LOG");
    debugDump("removing the XPCOM_MEM_LEAK_LOG environment variable... " +
              "previous value " + gEnvXPCOMMemLeakLog);
    env.set("XPCOM_MEM_LEAK_LOG", "");
  }

  if (env.exists("XPCOM_DEBUG_BREAK")) {
    gEnvXPCOMDebugBreak = env.get("XPCOM_DEBUG_BREAK");
    debugDump("setting the XPCOM_DEBUG_BREAK environment variable to " +
              "warn... previous value " + gEnvXPCOMDebugBreak);
  } else {
    debugDump("setting the XPCOM_DEBUG_BREAK environment variable to " +
              "warn... previously it didn't exist");
  }

  env.set("XPCOM_DEBUG_BREAK", "warn");

  if (gStageUpdate) {
    debugDump("setting the MOZ_UPDATE_STAGING environment variable to 1");
    env.set("MOZ_UPDATE_STAGING", "1");
  }

  debugDump("setting MOZ_NO_SERVICE_FALLBACK environment variable to 1");
  env.set("MOZ_NO_SERVICE_FALLBACK", "1");
}

/**
 * Sets the environment back to the original values after launching the
 * application.
 */
function resetEnvironment() {
  // Prevent resetting the environment more than once.
  if (gShouldResetEnv !== true) {
    return;
  }

  gShouldResetEnv = false;

  let env = Cc["@mozilla.org/process/environment;1"].
            getService(Ci.nsIEnvironment);

  if (gEnvXPCOMMemLeakLog) {
    debugDump("setting the XPCOM_MEM_LEAK_LOG environment variable back to " +
              gEnvXPCOMMemLeakLog);
    env.set("XPCOM_MEM_LEAK_LOG", gEnvXPCOMMemLeakLog);
  }

  if (gEnvXPCOMDebugBreak) {
    debugDump("setting the XPCOM_DEBUG_BREAK environment variable back to " +
              gEnvXPCOMDebugBreak);
    env.set("XPCOM_DEBUG_BREAK", gEnvXPCOMDebugBreak);
  } else {
    debugDump("clearing the XPCOM_DEBUG_BREAK environment variable");
    env.set("XPCOM_DEBUG_BREAK", "");
  }

  if (IS_UNIX) {
    if (IS_MACOSX) {
      if (gEnvDyldLibraryPath) {
        debugDump("setting DYLD_LIBRARY_PATH environment variable value " +
                  "back to " + gEnvDyldLibraryPath);
        env.set("DYLD_LIBRARY_PATH", gEnvDyldLibraryPath);
      } else {
        debugDump("removing DYLD_LIBRARY_PATH environment variable");
        env.set("DYLD_LIBRARY_PATH", "");
      }
    } else {
      if (gEnvLdLibraryPath) {
        debugDump("setting LD_LIBRARY_PATH environment variable value back " +
                  "to " + gEnvLdLibraryPath);
        env.set("LD_LIBRARY_PATH", gEnvLdLibraryPath);
      } else {
        debugDump("removing LD_LIBRARY_PATH environment variable");
        env.set("LD_LIBRARY_PATH", "");
      }
    }
  }

  if (IS_WIN && gAddedEnvXRENoWindowsCrashDialog) {
    debugDump("removing the XRE_NO_WINDOWS_CRASH_DIALOG environment " +
              "variable");
    env.set("XRE_NO_WINDOWS_CRASH_DIALOG", "");
  }

  if (gStageUpdate) {
    debugDump("removing the MOZ_UPDATE_STAGING environment variable");
    env.set("MOZ_UPDATE_STAGING", "");
  }

  debugDump("removing MOZ_NO_SERVICE_FALLBACK environment variable");
  env.set("MOZ_NO_SERVICE_FALLBACK", "");
}
