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

"use strict";
/* eslint-disable no-undef */

const { classes: Cc, interfaces: Ci, manager: Cm, results: Cr,
        utils: Cu } = Components;

const URL_HTTP_UPDATE_SJS = "http://test_details/";

/* global INSTALL_LOCALE, MOZ_APP_NAME, BIN_SUFFIX, MOZ_APP_VENDOR */
/* global MOZ_APP_BASENAME, APP_BIN_SUFFIX, APP_INFO_NAME, APP_INFO_VENDOR */
/* global IS_WIN, IS_MACOSX, IS_UNIX, MOZ_VERIFY_MAR_SIGNATURE */
/* global IS_AUTHENTICODE_CHECK_ENABLED */
load("../data/xpcshellConstantsPP.js");

function getLogSuffix() {
  if (IS_WIN) {
    return "_win";
  }
  if (IS_MACOSX) {
    return "_mac";
  }
  return "_linux";
}

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/ctypes.jsm", this);

const DIR_MACOS = IS_MACOSX ? "Contents/MacOS/" : "";
const DIR_RESOURCES = IS_MACOSX ? "Contents/Resources/" : "";
const TEST_FILE_SUFFIX = IS_MACOSX ? "_mac" : "";
const FILE_COMPLETE_MAR = "complete" + TEST_FILE_SUFFIX + ".mar";
const FILE_PARTIAL_MAR = "partial" + TEST_FILE_SUFFIX + ".mar";
const FILE_COMPLETE_PRECOMPLETE = "complete_precomplete" + TEST_FILE_SUFFIX;
const FILE_PARTIAL_PRECOMPLETE = "partial_precomplete" + TEST_FILE_SUFFIX;
const FILE_COMPLETE_REMOVEDFILES = "complete_removed-files" + TEST_FILE_SUFFIX;
const FILE_PARTIAL_REMOVEDFILES = "partial_removed-files" + TEST_FILE_SUFFIX;
const FILE_UPDATE_IN_PROGRESS_LOCK = "updated.update_in_progress.lock";
const COMPARE_LOG_SUFFIX = getLogSuffix();
const LOG_COMPLETE_SUCCESS = "complete_log_success" + COMPARE_LOG_SUFFIX;
const LOG_PARTIAL_SUCCESS = "partial_log_success" + COMPARE_LOG_SUFFIX;
const LOG_PARTIAL_FAILURE = "partial_log_failure" + COMPARE_LOG_SUFFIX;
const LOG_REPLACE_SUCCESS = "replace_log_success";

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

const PERFORMING_STAGED_UPDATE = "Performing a staged update";
const CALL_QUIT = "calling QuitProgressUI";
const REMOVE_OLD_DIST_DIR = "removing old distribution directory";
const MOVE_OLD_DIST_DIR = "Moving old distribution directory to new location";
const ERR_UPDATE_IN_PROGRESS = "Update already in progress! Exiting";
const ERR_RENAME_FILE = "rename_file: failed to rename file";
const ERR_ENSURE_COPY = "ensure_copy: failed to copy the file";
const ERR_UNABLE_OPEN_DEST = "unable to open destination file";
const ERR_BACKUP_DISCARD = "backup_discard: unable to remove";
const ERR_MOVE_DESTDIR_7 = "Moving destDir to tmpDir failed, err: 7";
const ERR_BACKUP_CREATE_7 = "backup_create failed: 7";
const ERR_LOADSOURCEFILE_FAILED = "LoadSourceFile failed";
const ERR_PARENT_PID_PERSISTS = "The parent process didn't exit! Continuing with update.";

const LOG_SVC_SUCCESSFUL_LAUNCH = "Process was started... waiting on result.";

// Typical end of a message when calling assert
const MSG_SHOULD_EQUAL = " should equal the expected value";
const MSG_SHOULD_EXIST = "the file or directory should exist";
const MSG_SHOULD_NOT_EXIST = "the file or directory should not exist";

// All we care about is that the last modified time has changed so that Mac OS
// X Launch Services invalidates its cache so the test allows up to one minute
// difference in the last modified time.
const MAC_MAX_TIME_DIFFERENCE = 60000;

// How many of do_execute_soon calls to wait before the test is aborted.
const MAX_TIMEOUT_RUNS = 20000;

// Time in seconds the helper application should sleep before exiting. The
// helper can also be made to exit by writing |finish| to its input file.
const HELPER_SLEEP_TIMEOUT = 180;

// Maximum number of milliseconds the process that is launched can run before
// the test will try to kill it.
const APP_TIMER_TIMEOUT = 120000;

// How many of do_timeout calls using FILE_IN_USE_TIMEOUT_MS to wait before the
// test is aborted.
const FILE_IN_USE_MAX_TIMEOUT_RUNS = 60;
const FILE_IN_USE_TIMEOUT_MS = 1000;

const PIPE_TO_NULL = IS_WIN ? ">nul" : "> /dev/null 2>&1";

const LOG_FUNCTION = do_print;

const gHTTPHandlerPath = "updates.xml";

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

var gPIDPersistProcess;

// Variables are used instead of contants so tests can override these values if
// necessary.
var gCallbackBinFile = "callback_app" + BIN_SUFFIX;
var gCallbackArgs = ["./", "callback.log", "Test Arg 2", "Test Arg 3"];
var gPostUpdateBinFile = "postup_app" + BIN_SUFFIX;
var gSvcOriginalLogContents;
// Some update staging failures can remove the update. This allows tests to
// specify that the status file and the active update should not be checked
// after an update is staged.
var gStagingRemovedUpdate = false;

var gTimeoutRuns = 0;
var gFileInUseTimeoutRuns = 0;

// Environment related globals
var gShouldResetEnv = undefined;
var gAddedEnvXRENoWindowsCrashDialog = false;
var gEnvXPCOMDebugBreak;
var gEnvXPCOMMemLeakLog;
var gEnvDyldLibraryPath;
var gEnvLdLibraryPath;
var gASanOptions;

// Set to true to log additional information for debugging. To log additional
// information for an individual test set DEBUG_AUS_TEST to true in the test's
// run_test function.
var DEBUG_AUS_TEST = true;

const DATA_URI_SPEC = Services.io.newFileURI(do_get_file("../data", false)).spec;
/* import-globals-from ../data/shared.js */
Services.scriptloader.loadSubScript(DATA_URI_SPEC + "shared.js", this);

var gTestFiles = [];
var gTestDirs = [];

// Common files for both successful and failed updates.
var gTestFilesCommon = [
  {
    description: "Should never change",
    fileName: FILE_UPDATE_SETTINGS_INI,
    relPathDir: DIR_RESOURCES,
    originalContents: UPDATE_SETTINGS_CONTENTS,
    compareContents: UPDATE_SETTINGS_CONTENTS,
    originalFile: null,
    compareFile: null,
    originalPerms: 0o767,
    comparePerms: 0o767
  }, {
    description: "Should never change",
    fileName: "channel-prefs.js",
    relPathDir: DIR_RESOURCES + "defaults/pref/",
    originalContents: "ShouldNotBeReplaced\n",
    compareContents: "ShouldNotBeReplaced\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o767,
    comparePerms: 0o767
  }];

  // Files for a complete successful update. This can be used for a complete
  // failed update by calling setTestFilesAndDirsForFailure.
var gTestFilesCompleteSuccess = [
  {
    description: "Added by update.manifest (add)",
    fileName: "precomplete",
    relPathDir: DIR_RESOURCES,
    originalContents: null,
    compareContents: null,
    originalFile: FILE_PARTIAL_PRECOMPLETE,
    compareFile: FILE_COMPLETE_PRECOMPLETE,
    originalPerms: 0o666,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "searchpluginstext0",
    relPathDir: DIR_RESOURCES + "searchplugins/",
    originalContents: "ToBeReplacedWithFromComplete\n",
    compareContents: "FromComplete\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o775,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "searchpluginspng1.png",
    relPathDir: DIR_RESOURCES + "searchplugins/",
    originalContents: null,
    compareContents: null,
    originalFile: null,
    compareFile: "complete.png",
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "searchpluginspng0.png",
    relPathDir: DIR_RESOURCES + "searchplugins/",
    originalContents: null,
    compareContents: null,
    originalFile: "partial.png",
    compareFile: "complete.png",
    originalPerms: 0o666,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "removed-files",
    relPathDir: DIR_RESOURCES,
    originalContents: null,
    compareContents: null,
    originalFile: FILE_PARTIAL_REMOVEDFILES,
    compareFile: FILE_COMPLETE_REMOVEDFILES,
    originalPerms: 0o666,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions1text0",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions1/",
    originalContents: null,
    compareContents: "FromComplete\n",
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions1png1.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions1/",
    originalContents: null,
    compareContents: null,
    originalFile: "partial.png",
    compareFile: "complete.png",
    originalPerms: 0o666,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions1png0.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions1/",
    originalContents: null,
    compareContents: null,
    originalFile: null,
    compareFile: "complete.png",
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions0text0",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions0/",
    originalContents: "ToBeReplacedWithFromComplete\n",
    compareContents: "FromComplete\n",
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions0png1.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions0/",
    originalContents: null,
    compareContents: null,
    originalFile: null,
    compareFile: "complete.png",
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions0png0.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions0/",
    originalContents: null,
    compareContents: null,
    originalFile: null,
    compareFile: "complete.png",
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "exe0.exe",
    relPathDir: DIR_MACOS,
    originalContents: null,
    compareContents: null,
    originalFile: FILE_HELPER_BIN,
    compareFile: FILE_COMPLETE_EXE,
    originalPerms: 0o777,
    comparePerms: 0o755
  }, {
    description: "Added by update.manifest (add)",
    fileName: "10text0",
    relPathDir: DIR_RESOURCES + "1/10/",
    originalContents: "ToBeReplacedWithFromComplete\n",
    compareContents: "FromComplete\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o767,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "0exe0.exe",
    relPathDir: DIR_RESOURCES + "0/",
    originalContents: null,
    compareContents: null,
    originalFile: FILE_HELPER_BIN,
    compareFile: FILE_COMPLETE_EXE,
    originalPerms: 0o777,
    comparePerms: 0o755
  }, {
    description: "Added by update.manifest (add)",
    fileName: "00text1",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: "ToBeReplacedWithFromComplete\n",
    compareContents: "FromComplete\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o677,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "00text0",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: "ToBeReplacedWithFromComplete\n",
    compareContents: "FromComplete\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o775,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "00png0.png",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: null,
    compareContents: null,
    originalFile: null,
    compareFile: "complete.png",
    originalPerms: 0o776,
    comparePerms: 0o644
  }, {
    description: "Removed by precomplete (remove)",
    fileName: "20text0",
    relPathDir: DIR_RESOURCES + "2/20/",
    originalContents: "ToBeDeleted\n",
    compareContents: null,
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: null
  }, {
    description: "Removed by precomplete (remove)",
    fileName: "20png0.png",
    relPathDir: DIR_RESOURCES + "2/20/",
    originalContents: "ToBeDeleted\n",
    compareContents: null,
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: null
  }];

// Concatenate the common files to the end of the array.
gTestFilesCompleteSuccess = gTestFilesCompleteSuccess.concat(gTestFilesCommon);

// Files for a partial successful update. This can be used for a partial failed
// update by calling setTestFilesAndDirsForFailure.
var gTestFilesPartialSuccess = [
  {
    description: "Added by update.manifest (add)",
    fileName: "precomplete",
    relPathDir: DIR_RESOURCES,
    originalContents: null,
    compareContents: null,
    originalFile: FILE_COMPLETE_PRECOMPLETE,
    compareFile: FILE_PARTIAL_PRECOMPLETE,
    originalPerms: 0o666,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "searchpluginstext0",
    relPathDir: DIR_RESOURCES + "searchplugins/",
    originalContents: "ToBeReplacedWithFromPartial\n",
    compareContents: "FromPartial\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o775,
    comparePerms: 0o644
  }, {
    description: "Patched by update.manifest if the file exists (patch-if)",
    fileName: "searchpluginspng1.png",
    relPathDir: DIR_RESOURCES + "searchplugins/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o666,
    comparePerms: 0o666
  }, {
    description: "Patched by update.manifest if the file exists (patch-if)",
    fileName: "searchpluginspng0.png",
    relPathDir: DIR_RESOURCES + "searchplugins/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o666,
    comparePerms: 0o666
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions1text0",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions1/",
    originalContents: null,
    compareContents: "FromPartial\n",
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Patched by update.manifest if the parent directory exists (patch-if)",
    fileName: "extensions1png1.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions1/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o666,
    comparePerms: 0o666
  }, {
    description: "Patched by update.manifest if the parent directory exists (patch-if)",
    fileName: "extensions1png0.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions1/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o666,
    comparePerms: 0o666
  }, {
    description: "Added by update.manifest if the parent directory exists (add-if)",
    fileName: "extensions0text0",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions0/",
    originalContents: "ToBeReplacedWithFromPartial\n",
    compareContents: "FromPartial\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o644,
    comparePerms: 0o644
  }, {
    description: "Patched by update.manifest if the parent directory exists (patch-if)",
    fileName: "extensions0png1.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions0/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o644,
    comparePerms: 0o644
  }, {
    description: "Patched by update.manifest if the parent directory exists (patch-if)",
    fileName: "extensions0png0.png",
    relPathDir: DIR_RESOURCES + "distribution/extensions/extensions0/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o644,
    comparePerms: 0o644
  }, {
    description: "Patched by update.manifest (patch)",
    fileName: "exe0.exe",
    relPathDir: DIR_MACOS,
    originalContents: null,
    compareContents: null,
    originalFile: FILE_COMPLETE_EXE,
    compareFile: FILE_PARTIAL_EXE,
    originalPerms: 0o755,
    comparePerms: 0o755
  }, {
    description: "Patched by update.manifest (patch)",
    fileName: "0exe0.exe",
    relPathDir: DIR_RESOURCES + "0/",
    originalContents: null,
    compareContents: null,
    originalFile: FILE_COMPLETE_EXE,
    compareFile: FILE_PARTIAL_EXE,
    originalPerms: 0o755,
    comparePerms: 0o755
  }, {
    description: "Added by update.manifest (add)",
    fileName: "00text0",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: "ToBeReplacedWithFromPartial\n",
    compareContents: "FromPartial\n",
    originalFile: null,
    compareFile: null,
    originalPerms: 0o644,
    comparePerms: 0o644
  }, {
    description: "Patched by update.manifest (patch)",
    fileName: "00png0.png",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: null,
    compareContents: null,
    originalFile: "complete.png",
    compareFile: "partial.png",
    originalPerms: 0o666,
    comparePerms: 0o666
  }, {
    description: "Added by update.manifest (add)",
    fileName: "20text0",
    relPathDir: DIR_RESOURCES + "2/20/",
    originalContents: null,
    compareContents: "FromPartial\n",
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "20png0.png",
    relPathDir: DIR_RESOURCES + "2/20/",
    originalContents: null,
    compareContents: null,
    originalFile: null,
    compareFile: "partial.png",
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Added by update.manifest (add)",
    fileName: "00text2",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: null,
    compareContents: "FromPartial\n",
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: 0o644
  }, {
    description: "Removed by update.manifest (remove)",
    fileName: "10text0",
    relPathDir: DIR_RESOURCES + "1/10/",
    originalContents: "ToBeDeleted\n",
    compareContents: null,
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: null
  }, {
    description: "Removed by update.manifest (remove)",
    fileName: "00text1",
    relPathDir: DIR_RESOURCES + "0/00/",
    originalContents: "ToBeDeleted\n",
    compareContents: null,
    originalFile: null,
    compareFile: null,
    originalPerms: null,
    comparePerms: null
  }];

// Concatenate the common files to the end of the array.
gTestFilesPartialSuccess = gTestFilesPartialSuccess.concat(gTestFilesCommon);

var gTestDirsCommon = [
  {
    relPathDir: DIR_RESOURCES + "3/",
    dirRemoved: false,
    files: ["3text0", "3text1"],
    filesRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "4/",
    dirRemoved: true,
    files: ["4text0", "4text1"],
    filesRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "5/",
    dirRemoved: true,
    files: ["5test.exe", "5text0", "5text1"],
    filesRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "6/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "7/",
    dirRemoved: true,
    files: ["7text0", "7text1"],
    subDirs: ["70/", "71/"],
    subDirFiles: ["7xtest.exe", "7xtext0", "7xtext1"]
  }, {
    relPathDir: DIR_RESOURCES + "8/",
    dirRemoved: false
  }, {
    relPathDir: DIR_RESOURCES + "8/80/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "8/81/",
    dirRemoved: false,
    files: ["81text0", "81text1"]
  }, {
    relPathDir: DIR_RESOURCES + "8/82/",
    dirRemoved: false,
    subDirs: ["820/", "821/"]
  }, {
    relPathDir: DIR_RESOURCES + "8/83/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "8/84/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "8/85/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "8/86/",
    dirRemoved: true,
    files: ["86text0", "86text1"]
  }, {
    relPathDir: DIR_RESOURCES + "8/87/",
    dirRemoved: true,
    subDirs: ["870/", "871/"],
    subDirFiles: ["87xtext0", "87xtext1"]
  }, {
    relPathDir: DIR_RESOURCES + "8/88/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "8/89/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "9/90/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "9/91/",
    dirRemoved: false,
    files: ["91text0", "91text1"]
  }, {
    relPathDir: DIR_RESOURCES + "9/92/",
    dirRemoved: false,
    subDirs: ["920/", "921/"]
  }, {
    relPathDir: DIR_RESOURCES + "9/93/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "9/94/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "9/95/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "9/96/",
    dirRemoved: true,
    files: ["96text0", "96text1"]
  }, {
    relPathDir: DIR_RESOURCES + "9/97/",
    dirRemoved: true,
    subDirs: ["970/", "971/"],
    subDirFiles: ["97xtext0", "97xtext1"]
  }, {
    relPathDir: DIR_RESOURCES + "9/98/",
    dirRemoved: true
  }, {
    relPathDir: DIR_RESOURCES + "9/99/",
    dirRemoved: true
  }];

// Directories for a complete successful update. This array can be used for a
// complete failed update by calling setTestFilesAndDirsForFailure.
var gTestDirsCompleteSuccess = [
  {
    description: "Removed by precomplete (rmdir)",
    relPathDir: DIR_RESOURCES + "2/20/",
    dirRemoved: true
  }, {
    description: "Removed by precomplete (rmdir)",
    relPathDir: DIR_RESOURCES + "2/",
    dirRemoved: true
  }];

// Concatenate the common files to the beginning of the array.
gTestDirsCompleteSuccess = gTestDirsCommon.concat(gTestDirsCompleteSuccess);

// Directories for a partial successful update. This array can be used for a
// partial failed update by calling setTestFilesAndDirsForFailure.
var gTestDirsPartialSuccess = [
  {
    description: "Removed by update.manifest (rmdir)",
    relPathDir: DIR_RESOURCES + "1/10/",
    dirRemoved: true
  }, {
    description: "Removed by update.manifest (rmdir)",
    relPathDir: DIR_RESOURCES + "1/",
    dirRemoved: true
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

  Assert.strictEqual(gTestID, undefined,
                     "gTestID should be 'undefined' (setupTestCommon should " +
                     "only be called once)");

  let caller = Components.stack.caller;
  gTestID = caller.filename.toString().split("/").pop().split(".")[0];

  createAppInfo("xpcshell@tests.mozilla.org", APP_INFO_NAME, "1.0", "2.0");

  // Tests that don't work with XULRunner.
  const XUL_RUNNER_INCOMPATIBLE = ["marAppApplyUpdateAppBinInUseStageSuccess_win",
                                   "marAppApplyUpdateStageSuccess",
                                   "marAppApplyUpdateSuccess",
                                   "marAppApplyUpdateAppBinInUseStageSuccessSvc_win",
                                   "marAppApplyUpdateStageSuccessSvc",
                                   "marAppApplyUpdateSuccessSvc"];
  // Replace with Array.prototype.includes when it has stabilized.
  if (MOZ_APP_NAME == "xulrunner" &&
      XUL_RUNNER_INCOMPATIBLE.indexOf(gTestID) != -1) {
    logTestInfo("Unable to run this test on xulrunner");
    return false;
  }

  if (IS_SERVICE_TEST && !shouldRunServiceTest()) {
    return false;
  }

  do_test_pending();

  setDefaultPrefs();

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
      // When the application doesn't exit properly it can cause the test to
      // fail again on the second run with an NS_ERROR_FILE_ACCESS_DENIED error
      // along with no useful information in the test log. To prevent this use
      // a different directory for the test when it isn't possible to remove the
      // existing test directory (bug 1294196).
      gTestID += "_new";
      logTestInfo("using a new directory for the test by changing gTestID " +
                  "since there is an existing test directory that can't be " +
                  "removed, gTestID: " + gTestID);
    }
  }

  if (IS_WIN) {
    Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, !!IS_SERVICE_TEST);
  }

  // adjustGeneralPaths registers a cleanup function that calls end_test when
  // it is defined as a function.
  adjustGeneralPaths();
  // Logged once here instead of in the mock directory provider to lessen test
  // log spam.
  debugDump("Updates Directory (UpdRootD) Path: " + getMockUpdRootD().path);

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
  return true;
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

  // The updates directory is located outside of the application directory and
  // needs to be removed on Windows and Mac OS X.
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
              if (e == Cr.NS_ERROR_FILE_DIR_NOT_EMPTY) {
                break;
              }
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
}

/**
 * Helper function that calls do_test_finished that tracks whether a parallel
 * run of a test passed when it runs synchronously so the log output can be
 * inspected.
 */
function doTestFinish() {
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
  if (DEBUG_AUS_TEST) {
    // Enable Update logging
    Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG, true);
  } else {
    // Some apps set this preference to true by default
    Services.prefs.setBoolPref(PREF_APP_UPDATE_LOG, false);
  }
  // In case telemetry is enabled for xpcshell tests.
  Services.prefs.setBoolPref(PREF_TOOLKIT_TELEMETRY_ENABLED, false);
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
    Assert.ok(postUpdateRunningFile.exists(),
              MSG_SHOULD_EXIST + getMsgPath(postUpdateRunningFile.path));
  } else {
    Assert.ok(!postUpdateRunningFile.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(postUpdateRunningFile.path));
  }
}

/**
 * Initializes the most commonly used settings and creates an instance of the
 * update service stub.
 */
function standardInit() {
  // Initialize the update service stub component
  initUpdateServiceStub();
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
  Assert.ok(iniFile.exists(),
            MSG_SHOULD_EXIST + getMsgPath(iniFile.path));
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
      Assert.ok(file.exists(),
                MSG_SHOULD_EXIST + getMsgPath(file.path));
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

/**
 * Get the nsILocalFile for a Windows special folder determined by the CSIDL
 * passed.
 *
 * @param   aCSIDL
 *          The CSIDL for the Windows special folder.
 * @return  The nsILocalFile for the Windows special folder.
 * @throws  If called from a platform other than Windows.
 */
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
  SHGetSpecialFolderPath(0, aryPath, aCSIDL, false);
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

XPCOMUtils.defineLazyGetter(this, "gInstallDirPathHash", function test_gIDPH() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

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

XPCOMUtils.defineLazyGetter(this, "gLocalAppDataDir", function test_gLADD() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  const CSIDL_LOCAL_APPDATA = 0x1c;
  return getSpecialFolderDir(CSIDL_LOCAL_APPDATA);
});

XPCOMUtils.defineLazyGetter(this, "gProgFilesDir", function test_gPFD() {
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
  return updatesDir;
}

XPCOMUtils.defineLazyGetter(this, "gUpdatesRootDir", function test_gURD() {
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
  return updatesDir;
}

/**
 * Creates an update in progress lock file in the specified directory on
 * Windows.
 *
 * @param   aDir
 *          The nsIFile for the directory where the lock file should be created.
 */
function createUpdateInProgressLockFile(aDir) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let file = aDir.clone();
  file.append(FILE_UPDATE_IN_PROGRESS_LOCK);
  file.create(file.NORMAL_FILE_TYPE, 0o444);
  file.QueryInterface(Ci.nsILocalFileWin);
  file.fileAttributesWin |= file.WFA_READONLY;
  file.fileAttributesWin &= ~file.WFA_READWRITE;
  Assert.ok(file.exists(),
            MSG_SHOULD_EXIST + getMsgPath(file.path));
  Assert.ok(!file.isWritable(),
            "the lock file should not be writeable");
}

/**
 * Removes an update in progress lock file in the specified directory on
 * Windows.
 *
 * @param   aDir
 *          The nsIFile for the directory where the lock file is located.
 */
function removeUpdateInProgressLockFile(aDir) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let file = aDir.clone();
  file.append(FILE_UPDATE_IN_PROGRESS_LOCK);
  file.QueryInterface(Ci.nsILocalFileWin);
  file.fileAttributesWin |= file.WFA_READWRITE;
  file.fileAttributesWin &= ~file.WFA_READONLY;
  file.remove(false);
  Assert.ok(!file.exists(),
            MSG_SHOULD_NOT_EXIST + getMsgPath(file.path));
}

/**
 * Gets the test updater from the test data direcory.
 *
 * @return  nsIFIle for the test updater.
 */
function getTestUpdater() {
  let updater = getTestDirFile("updater.app", true);
  if (!updater.exists()) {
    updater = getTestDirFile(FILE_UPDATER_BIN);
    if (!updater.exists()) {
      do_throw("Unable to find the updater binary!");
    }
  }
  Assert.ok(updater.exists(),
            MSG_SHOULD_EXIST + getMsgPath(updater.path));
  return updater;
}

/**
 * Copies the test updater to the GRE binary directory and returns the nsIFile
 * for the copied test updater.
 *
 * @return  nsIFIle for the copied test updater.
 */
function copyTestUpdaterToBinDir() {
  let testUpdater = getTestUpdater();
  let updater = getGREBinDir();
  updater.append(testUpdater.leafName);
  if (!updater.exists()) {
    testUpdater.copyToFollowingLinks(updater.parent, updater.leafName);
  }
  return updater;
}

/**
 * Copies the test updater to the location where it will be launched to apply an
 * update and returns the nsIFile for the copied test updater.
 *
 * @return  nsIFIle for the copied test updater.
 */
function copyTestUpdaterForRunUsingUpdater() {
  if (IS_WIN) {
    return copyTestUpdaterToBinDir();
  }

  let testUpdater = getTestUpdater();
  let updater = getUpdatesPatchDir();
  updater.append(testUpdater.leafName);
  if (!updater.exists()) {
    testUpdater.copyToFollowingLinks(updater.parent, updater.leafName);
  }

  if (IS_MACOSX) {
    updater.append("Contents");
    updater.append("MacOS");
    updater.append("org.mozilla.updater");
  }
  return updater;
}

/**
 * Logs the contents of an update log and for maintenance service tests this
 * will log the contents of the latest maintenanceservice.log.
 *
 * @param   aLogLeafName
 *          The leaf name of the update log.
 */
function logUpdateLog(aLogLeafName) {
  let updateLog = getUpdateLog(aLogLeafName);
  if (updateLog.exists()) {
    // xpcshell tests won't display the entire contents so log each line.
    let updateLogContents = readFileBytes(updateLog).replace(/\r\n/g, "\n");
    updateLogContents = replaceLogPaths(updateLogContents);
    let aryLogContents = updateLogContents.split("\n");
    logTestInfo("contents of " + updateLog.path + ":");
    aryLogContents.forEach(function RU_LC_FE(aLine) {
      logTestInfo(aLine);
    });
  } else {
    logTestInfo("update log doesn't exist, path: " + updateLog.path);
  }

  if (IS_SERVICE_TEST) {
    let serviceLog = getMaintSvcDir();
    serviceLog.append("logs");
    serviceLog.append("maintenanceservice.log");
    if (serviceLog.exists()) {
      // xpcshell tests won't display the entire contents so log each line.
      let serviceLogContents = readFileBytes(serviceLog).replace(/\r\n/g, "\n");
      serviceLogContents = replaceLogPaths(serviceLogContents);
      let aryLogContents = serviceLogContents.split("\n");
      logTestInfo("contents of " + serviceLog.path + ":");
      aryLogContents.forEach(function RU_LC_FE(aLine) {
        logTestInfo(aLine);
      });
    } else {
      logTestInfo("maintenance service log doesn't exist, path: " +
                  serviceLog.path);
    }
  }
}

/**
 * Gets the maintenance service log contents.
 */
function readServiceLogFile() {
  let file = getMaintSvcDir();
  file.append("logs");
  file.append("maintenanceservice.log");
  return readFile(file);
}

/**
 * Launches the updater binary to apply an update for updater tests.
 *
 * @param   aExpectedStatus
 *          The expected value of update.status when the test finishes. For
 *          service tests passing STATE_PENDING or STATE_APPLIED will change the
 *          value to STATE_PENDING_SVC and STATE_APPLIED_SVC respectively.
 * @param   aSwitchApp
 *          If true the update should switch the application with an updated
 *          staged application and if false the update should be applied to the
 *          installed application.
 * @param   aExpectedExitValue
 *          The expected exit value from the updater binary for non-service
 *          tests.
 * @param   aCheckSvcLog
 *          Whether the service log should be checked for service tests.
 * @param   aPatchDirPath (optional)
 *          When specified the patch directory path to use for invalid argument
 *          tests otherwise the normal path will be used.
 * @param   aInstallDirPath (optional)
 *          When specified the install directory path to use for invalid
 *          argument tests otherwise the normal path will be used.
 * @param   aApplyToDirPath (optional)
 *          When specified the apply to / working directory path to use for
 *          invalid argument tests otherwise the normal path will be used.
 * @param   aCallbackPath (optional)
 *          When specified the callback path to use for invalid argument tests
 *          otherwise the normal path will be used.
 */
function runUpdate(aExpectedStatus, aSwitchApp, aExpectedExitValue, aCheckSvcLog,
                   aPatchDirPath, aInstallDirPath, aApplyToDirPath,
                   aCallbackPath) {
  let isInvalidArgTest = !!aPatchDirPath || !!aInstallDirPath ||
                         !!aApplyToDirPath || aCallbackPath;

  let svcOriginalLog;
  if (IS_SERVICE_TEST) {
    copyFileToTestAppDir(FILE_MAINTENANCE_SERVICE_BIN, false);
    copyFileToTestAppDir(FILE_MAINTENANCE_SERVICE_INSTALLER_BIN, false);
    if (aCheckSvcLog) {
      svcOriginalLog = readServiceLogFile();
    }
  }

  let pid = 0;
  if (gPIDPersistProcess) {
    pid = gPIDPersistProcess.pid;
    gEnv.set("MOZ_TEST_SHORTER_WAIT_PID", "1");
  }

  // Copy the updater binary to the directory where it will apply updates.
  let updateBin = copyTestUpdaterForRunUsingUpdater();
  Assert.ok(updateBin.exists(),
            MSG_SHOULD_EXIST + getMsgPath(updateBin.path));

  let updatesDirPath = aPatchDirPath || getUpdatesPatchDir().path;
  let installDirPath = aInstallDirPath || getApplyDirFile(null, true).path;
  let applyToDirPath = aApplyToDirPath || getApplyDirFile(null, true).path;
  let stageDirPath = aApplyToDirPath || getStageDirFile(null, true).path;

  let callbackApp = getApplyDirFile(DIR_RESOURCES + gCallbackBinFile);
  callbackApp.permissions = PERMS_DIRECTORY;

  setAppBundleModTime();

  let args = [updatesDirPath, installDirPath];
  if (aSwitchApp) {
    args[2] = stageDirPath;
    args[3] = pid + "/replace";
  } else {
    args[2] = applyToDirPath;
    args[3] = pid;
  }

  let launchBin = IS_SERVICE_TEST && isInvalidArgTest ? callbackApp : updateBin;

  if (!isInvalidArgTest) {
    args = args.concat([callbackApp.parent.path, callbackApp.path]);
    args = args.concat(gCallbackArgs);
  } else if (IS_SERVICE_TEST) {
    args = ["launch-service", updateBin.path].concat(args);
  } else if (aCallbackPath) {
    args = args.concat([callbackApp.parent.path, aCallbackPath]);
  }

  debugDump("launching the program: " + launchBin.path + " " + args.join(" "));

  if (aSwitchApp && !isInvalidArgTest) {
    // We want to set the env vars again
    gShouldResetEnv = undefined;
  }

  setEnvironment();

  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
  process.init(launchBin);
  process.run(true, args, args.length);

  resetEnvironment();

  if (gPIDPersistProcess) {
    gEnv.set("MOZ_TEST_SHORTER_WAIT_PID", "");
  }

  let status = readStatusFile();
  if ((!IS_SERVICE_TEST && process.exitValue != aExpectedExitValue) ||
      status != aExpectedStatus) {
    if (process.exitValue != aExpectedExitValue) {
      logTestInfo("updater exited with unexpected value! Got: " +
                  process.exitValue + ", Expected: " + aExpectedExitValue);
    }
    if (status != aExpectedStatus) {
      logTestInfo("update status is not the expected status! Got: " + status +
                  ", Expected: " + aExpectedStatus);
    }
    logUpdateLog(FILE_LAST_UPDATE_LOG);
  }

  if (!IS_SERVICE_TEST) {
    Assert.equal(process.exitValue, aExpectedExitValue,
                 "the process exit value" + MSG_SHOULD_EQUAL);
  }
  Assert.equal(status, aExpectedStatus,
               "the update status" + MSG_SHOULD_EQUAL);

  if (IS_SERVICE_TEST && aCheckSvcLog) {
    let contents = readServiceLogFile();
    Assert.notEqual(contents, svcOriginalLog,
                    "the contents of the maintenanceservice.log should not " +
                    "be the same as the original contents");
    if (!isInvalidArgTest) {
      Assert.notEqual(contents.indexOf(LOG_SVC_SUCCESSFUL_LAUNCH), -1,
                      "the contents of the maintenanceservice.log should " +
                      "contain the successful launch string");
    }
  }

  do_execute_soon(runUpdateFinished);
}

/**
 * Launches the helper binary synchronously with the specified arguments for
 * updater tests.
 *
 * @param   aArgs
 *          The arguments to pass to the helper binary.
 * @return  the process exit value returned by the helper binary.
 */
function runTestHelperSync(aArgs) {
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let process = Cc["@mozilla.org/process/util;1"].
                createInstance(Ci.nsIProcess);
  process.init(helperBin);
  debugDump("Running " + helperBin.path + " " + aArgs.join(" "));
  process.run(true, aArgs, aArgs.length);
  return process.exitValue;
}

/**
 * Creates a symlink for updater tests.
 */
function createSymlink() {
  let args = ["setup-symlink", "moz-foo", "moz-bar", "target",
              getApplyDirFile().path + "/" + DIR_RESOURCES + "link"];
  let exitValue = runTestHelperSync(args);
  Assert.equal(exitValue, 0,
               "the helper process exit value should be 0");
  getApplyDirFile(DIR_RESOURCES + "link", false).permissions = 0o666;
  args = ["setup-symlink", "moz-foo2", "moz-bar2", "target2",
          getApplyDirFile().path + "/" + DIR_RESOURCES + "link2", "change-perm"];
  exitValue = runTestHelperSync(args);
  Assert.equal(exitValue, 0,
               "the helper process exit value should be 0");
}

/**
 * Removes a symlink for updater tests.
 */
function removeSymlink() {
  let args = ["remove-symlink", "moz-foo", "moz-bar", "target",
              getApplyDirFile().path + "/" + DIR_RESOURCES + "link"];
  let exitValue = runTestHelperSync(args);
  Assert.equal(exitValue, 0,
               "the helper process exit value should be 0");
  args = ["remove-symlink", "moz-foo2", "moz-bar2", "target2",
          getApplyDirFile().path + "/" + DIR_RESOURCES + "link2"];
  exitValue = runTestHelperSync(args);
  Assert.equal(exitValue, 0,
               "the helper process exit value should be 0");
}

/**
 * Checks a symlink for updater tests.
 */
function checkSymlink() {
  let args = ["check-symlink",
              getApplyDirFile().path + "/" + DIR_RESOURCES + "link"];
  let exitValue = runTestHelperSync(args);
  Assert.equal(exitValue, 0,
               "the helper process exit value should be 0");
}

/**
 * Sets the active update and related information for updater tests.
 */
function setupActiveUpdate() {
  let pendingState = IS_SERVICE_TEST ? STATE_PENDING_SVC : STATE_PENDING;
  let patchProps = {state: pendingState};
  let patches = getLocalPatchString(patchProps);
  let updates = getLocalUpdateString({}, patches);
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeVersionFile(DEFAULT_UPDATE_VERSION);
  writeStatusFile(pendingState);
  reloadUpdateManagerData();
  Assert.ok(!!gUpdateManager.activeUpdate,
            "the active update should be defined");
}

/**
 * Gets the specified update log.
 *
 * @param   aLogLeafName
 *          The leaf name of the log to get.
 * @return  nsIFile for the update log.
 */
function getUpdateLog(aLogLeafName) {
  let updateLog = getUpdatesDir();
  if (aLogLeafName == FILE_UPDATE_LOG) {
    updateLog.append(DIR_PATCH);
  }
  updateLog.append(aLogLeafName);
  return updateLog;
}

/**
 * The update-staged observer for the call to nsIUpdateProcessor:processUpdate.
 */
const gUpdateStagedObserver = {
  observe(aSubject, aTopic, aData) {
    debugDump("observe called with topic: " + aTopic + ", data: " + aData);
    if (aTopic == "update-staged") {
      Services.obs.removeObserver(gUpdateStagedObserver, "update-staged");
      // The environment is reset after the update-staged observer topic because
      // processUpdate in nsIUpdateProcessor uses a new thread and clearing the
      // environment immediately after calling processUpdate can clear the
      // environment before the updater is launched.
      resetEnvironment();
      // Use do_execute_soon to prevent any failures from propagating to the
      // update service.
      do_execute_soon(checkUpdateStagedState.bind(null, aData));
    }
  },
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
};

/**
 * Stages an update using nsIUpdateProcessor:processUpdate for updater tests.
 *
 * @param   aCheckSvcLog
 *          Whether the service log should be checked for service tests.
 */
function stageUpdate(aCheckSvcLog) {
  debugDump("start - attempting to stage update");

  if (IS_SERVICE_TEST && aCheckSvcLog) {
    gSvcOriginalLogContents = readServiceLogFile();
  }

  Services.obs.addObserver(gUpdateStagedObserver, "update-staged");

  setAppBundleModTime();
  setEnvironment();
  try {
    // Stage the update.
    Cc["@mozilla.org/updates/update-processor;1"].
      createInstance(Ci.nsIUpdateProcessor).
      processUpdate(gUpdateManager.activeUpdate);
  } catch (e) {
    Assert.ok(false,
              "error thrown while calling processUpdate, exception: " + e);
  }

  // The environment is not reset here because processUpdate in
  // nsIUpdateProcessor uses a new thread and clearing the environment
  // immediately after calling processUpdate can clear the environment before
  // the updater is launched. Instead it is reset after the update-staged
  // observer topic.

  debugDump("finish - attempting to stage update");
}

/**
 * Checks that the update state is correct as well as the expected files are
 * present after staging and update for updater tests and then calls
 * stageUpdateFinished.
 *
 * @param   aUpdateState
 *          The update state received by the observer notification.
 */
function checkUpdateStagedState(aUpdateState) {
  if (IS_WIN) {
    if (IS_SERVICE_TEST) {
      waitForServiceStop(false);
    } else {
      let updater = getApplyDirFile(FILE_UPDATER_BIN, true);
      if (isFileInUse(updater)) {
        do_timeout(FILE_IN_USE_TIMEOUT_MS,
                   checkUpdateStagedState.bind(null, aUpdateState));
        return;
      }
    }
  }

  Assert.equal(aUpdateState, STATE_AFTER_STAGE,
               "the notified state" + MSG_SHOULD_EQUAL);

  if (!gStagingRemovedUpdate) {
    Assert.equal(readStatusState(), STATE_AFTER_STAGE,
                 "the status file state" + MSG_SHOULD_EQUAL);

    Assert.equal(gUpdateManager.activeUpdate.state, STATE_AFTER_STAGE,
                 "the update state" + MSG_SHOULD_EQUAL);
  }

  Assert.equal(gUpdateManager.updateCount, 1,
               "the update manager updateCount attribute" + MSG_SHOULD_EQUAL);
  Assert.equal(gUpdateManager.getUpdateAt(0).state, STATE_AFTER_STAGE,
               "the update state" + MSG_SHOULD_EQUAL);

  let log = getUpdateLog(FILE_LAST_UPDATE_LOG);
  Assert.ok(log.exists(),
            MSG_SHOULD_EXIST + getMsgPath(log.path));

  log = getUpdateLog(FILE_UPDATE_LOG);
  Assert.ok(!log.exists(),
            MSG_SHOULD_NOT_EXIST + getMsgPath(log.path));

  log = getUpdateLog(FILE_BACKUP_UPDATE_LOG);
  Assert.ok(!log.exists(),
            MSG_SHOULD_NOT_EXIST + getMsgPath(log.path));

  let stageDir = getStageDirFile(null, true);
  if (STATE_AFTER_STAGE == STATE_APPLIED ||
      STATE_AFTER_STAGE == STATE_APPLIED_SVC) {
    Assert.ok(stageDir.exists(),
              MSG_SHOULD_EXIST + getMsgPath(stageDir.path));
  } else {
    Assert.ok(!stageDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(stageDir.path));
  }

  if (IS_SERVICE_TEST && gSvcOriginalLogContents !== undefined) {
    let contents = readServiceLogFile();
    Assert.notEqual(contents, gSvcOriginalLogContents,
                    "the contents of the maintenanceservice.log should not " +
                    "be the same as the original contents");
    Assert.notEqual(contents.indexOf(LOG_SVC_SUCCESSFUL_LAUNCH), -1,
                    "the contents of the maintenanceservice.log should " +
                    "contain the successful launch string");
  }

  do_execute_soon(stageUpdateFinished);
}

/**
 * Helper function to check whether the maintenance service updater tests should
 * run. See bug 711660 for more details.
 *
 * @return true if the test should run and false if it shouldn't.
 */
function shouldRunServiceTest() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let binDir = getGREBinDir();
  let updaterBin = binDir.clone();
  updaterBin.append(FILE_UPDATER_BIN);
  Assert.ok(updaterBin.exists(),
            MSG_SHOULD_EXIST + ", leafName: " + updaterBin.leafName);

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
  let args = ["wait-for-service-stop", "MozillaMaintenance", "10"];
  let exitValue = runTestHelperSync(args);
  Assert.notEqual(exitValue, 0xEE, "the maintenance service should be " +
                  "installed (if not, build system configuration bug?)");

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
 * @param   aBinPath
 *          The path to the file to check if it is signed.
 * @return  true if the file is signed and false if it isn't.
 */
function isBinarySigned(aBinPath) {
  let args = ["check-signature", aBinPath];
  let exitValue = runTestHelperSync(args);
  if (exitValue != 0) {
    logTestInfo("binary is not signed. " + FILE_HELPER_BIN + " returned " +
                exitValue + " for file " + aBinPath);
    return false;
  }
  return true;
}

/**
 * Helper function for asynchronously setting up the application files required
 * to launch the application for the updater tests by either copying or creating
 * symlinks for the files. This is needed for Windows debug builds which can
 * lock a file that is being copied so that the tests can run in parallel. After
 * the files have been copied the setupUpdaterTestFinished function will be
 * called.
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
    do_execute_soon(setupAppFilesAsync);
    return;
  }

  do_execute_soon(setupUpdaterTestFinished);
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
  let appFiles = [{relPath: FILE_APP_BIN,
                   inGreDir: false},
                  {relPath: FILE_APPLICATION_INI,
                   inGreDir: true},
                  {relPath: "dependentlibs.list",
                   inGreDir: true}];

  // On Linux the updater.png must also be copied
  if (IS_UNIX && !IS_MACOSX) {
    appFiles.push({relPath: "icons/updater.png",
                   inGreDir: true});
  }

  // Read the dependent libs file leafnames from the dependentlibs.list file
  // into the array.
  let deplibsFile = gGREDirOrig.clone();
  deplibsFile.append("dependentlibs.list");
  let fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  fis.init(deplibsFile, 0x01, 0o444, Ci.nsIFileInputStream.CLOSE_ON_EOF);
  fis.QueryInterface(Ci.nsILineInputStream);

  let hasMore;
  let line = {};
  do {
    hasMore = fis.readLine(line);
    appFiles.push({relPath: line.value,
                   inGreDir: false});
  } while (hasMore);

  fis.close();

  appFiles.forEach(function CMAF_FLN_FE(aAppFile) {
    copyFileToTestAppDir(aAppFile.relPath, aAppFile.inGreDir);
  });

  copyTestUpdaterToBinDir();

  debugDump("finish - copying or creating symlinks to application files " +
            "for the test");
}

/**
 * Copies the specified files from the dist/bin directory into the test's
 * application directory.
 *
 * @param   aFileRelPath
 *          The relative path to the source and the destination of the file to
 *          copy.
 * @param   aInGreDir
 *          Whether the file is located in the GRE directory which is
 *          <bundle>/Contents/Resources on Mac OS X and is the installation
 *          directory on all other platforms. If false the file must be in the
 *          GRE Binary directory which is <bundle>/Contents/MacOS on Mac OS X
 *          and is the installation directory on on all other platforms.
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
  if (!shouldSymlink) {
    if (!destFile.exists()) {
      try {
        srcFile.copyToFollowingLinks(destFile.parent, destFile.leafName);
      } catch (e) {
        // Just in case it is partially copied
        if (destFile.exists()) {
          try {
            destFile.remove(true);
          } catch (ex) {
            logTestInfo("unable to remove file that failed to copy! Path: " +
                        destFile.path);
          }
        }
        do_throw("Unable to copy file! Path: " + srcFile.path +
                 ", Exception: " + ex);
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
                destFile.leafName + " should be a symlink");
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
 *
 * @return true if the installed service is from this build. If the installed
 *         service is not from this build the test will fail instead of
 *         returning false.
 */
function attemptServiceInstall() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  let maintSvcDir = getMaintSvcDir();
  Assert.ok(maintSvcDir.exists(),
            MSG_SHOULD_EXIST + ", leafName: " + maintSvcDir.leafName);
  let oldMaintSvcBin = maintSvcDir.clone();
  oldMaintSvcBin.append(FILE_MAINTENANCE_SERVICE_BIN);
  Assert.ok(oldMaintSvcBin.exists(),
            MSG_SHOULD_EXIST + ", leafName: " + oldMaintSvcBin.leafName);
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
 * Waits for the applications that are launched by the maintenance service to
 * stop.
 */
function waitServiceApps() {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  // maintenanceservice_installer.exe is started async during updates.
  waitForApplicationStop("maintenanceservice_installer.exe");
  // maintenanceservice_tmp.exe is started async from the service installer.
  waitForApplicationStop("maintenanceservice_tmp.exe");
  // In case the SCM thinks the service is stopped, but process still exists.
  waitForApplicationStop("maintenanceservice.exe");
}

/**
 * Waits for the maintenance service to stop.
 */
function waitForServiceStop(aFailTest) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  waitServiceApps();
  debugDump("waiting for the maintenance service to stop if necessary");
  // Use the helper bin to ensure the service is stopped. If not stopped, then
  // wait for the service to stop (at most 120 seconds).
  let args = ["wait-for-service-stop", "MozillaMaintenance", "120"];
  let exitValue = runTestHelperSync(args);
  Assert.notEqual(exitValue, 0xEE,
                  "the maintenance service should exist");
  if (exitValue != 0) {
    if (aFailTest) {
      Assert.ok(false, "the maintenance service should stop, process exit " +
                "value: " + exitValue);
    }
    logTestInfo("maintenance service did not stop which may cause test " +
                "failures later, process exit value: " + exitValue);
  } else {
    debugDump("service stopped");
  }
  waitServiceApps();
}

/**
 * Waits for the specified application to stop.
 *
 * @param   aApplication
 *          The application binary name to wait until it has stopped.
 */
function waitForApplicationStop(aApplication) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  debugDump("waiting for " + aApplication + " to stop if necessary");
  // Use the helper bin to ensure the application is stopped. If not stopped,
  // then wait for it to stop (at most 120 seconds).
  let args = ["wait-for-application-exit", aApplication, "120"];
  let exitValue = runTestHelperSync(args);
  Assert.equal(exitValue, 0,
               "the process should have stopped, process name: " +
               aApplication);
}


/**
 * Gets the platform specific shell binary that is launched using nsIProcess and
 * in turn launches a binary used for the test (e.g. application, updater,
 * etc.). A shell is used so debug console output can be redirected to a file so
 * it doesn't end up in the test log.
 *
 * @return nsIFile for the shell binary to launch using nsIProcess.
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
  Assert.ok(launchBin.exists(),
            MSG_SHOULD_EXIST + getMsgPath(launchBin.path));

  return launchBin;
}


/**
 * Locks a Windows directory.
 *
 * @param   aDirPath
 *          The test file object that describes the file to make in use.
 */
function lockDirectory(aDirPath) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  debugDump("start - locking installation directory");
  const LPCWSTR = ctypes.char16_t.ptr;
  const DWORD = ctypes.uint32_t;
  const LPVOID = ctypes.voidptr_t;
  const GENERIC_READ = 0x80000000;
  const FILE_SHARE_READ = 1;
  const FILE_SHARE_WRITE = 2;
  const OPEN_EXISTING = 3;
  const FILE_FLAG_BACKUP_SEMANTICS = 0x02000000;
  const INVALID_HANDLE_VALUE = LPVOID(0xffffffff);
  let kernel32 = ctypes.open("kernel32");
  let CreateFile = kernel32.declare("CreateFileW", ctypes.winapi_abi,
                                    LPVOID, LPCWSTR, DWORD, DWORD,
                                    LPVOID, DWORD, DWORD, LPVOID);
  gHandle = CreateFile(aDirPath, GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE, LPVOID(0),
                       OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, LPVOID(0));
  Assert.notEqual(gHandle.toString(), INVALID_HANDLE_VALUE.toString(),
                  "the handle should not equal INVALID_HANDLE_VALUE");
  kernel32.close();
  debugDump("finish - locking installation directory");
}

/**
 * Launches the test helper binary to make it in use for updater tests and then
 * calls waitForHelperSleep.
 *
 * @param   aRelPath
 *          The relative path in the apply to directory for the helper binary.
 * @param   aCopyTestHelper
 *          Whether to copy the test helper binary to the relative path in the
 *          apply to directory.
 */
function runHelperFileInUse(aRelPath, aCopyTestHelper) {
  debugDump("aRelPath: " + aRelPath);
  // Launch an existing file so it is in use during the update.
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let fileInUseBin = getApplyDirFile(aRelPath);
  if (aCopyTestHelper) {
    if (fileInUseBin.exists()) {
      fileInUseBin.remove(false);
    }
    helperBin.copyTo(fileInUseBin.parent, fileInUseBin.leafName);
  }
  fileInUseBin.permissions = PERMS_DIRECTORY;
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  let fileInUseProcess = Cc["@mozilla.org/process/util;1"].
                         createInstance(Ci.nsIProcess);
  fileInUseProcess.init(fileInUseBin);
  fileInUseProcess.run(false, args, args.length);

  do_execute_soon(waitForHelperSleep);
}

/**
 * Launches the test helper binary to provide a pid that is in use for updater
 * tests and then calls waitForHelperSleep.
 *
 * @param   aRelPath
 *          The relative path in the apply to directory for the helper binary.
 * @param   aCopyTestHelper
 *          Whether to copy the test helper binary to the relative path in the
 *          apply to directory.
 */
function runHelperPIDPersists(aRelPath, aCopyTestHelper) {
  debugDump("aRelPath: " + aRelPath);
  // Launch an existing file so it is in use during the update.
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let pidPersistsBin = getApplyDirFile(aRelPath);
  if (aCopyTestHelper) {
    if (pidPersistsBin.exists()) {
      pidPersistsBin.remove(false);
    }
    helperBin.copyTo(pidPersistsBin.parent, pidPersistsBin.leafName);
  }
  pidPersistsBin.permissions = PERMS_DIRECTORY;
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT];
  gPIDPersistProcess = Cc["@mozilla.org/process/util;1"].
                       createInstance(Ci.nsIProcess);
  gPIDPersistProcess.init(pidPersistsBin);
  gPIDPersistProcess.run(false, args, args.length);

  do_execute_soon(waitForHelperSleep);
}

/**
 * Launches the test helper binary and locks a file specified on the command
 * line for updater tests and then calls waitForHelperSleep.
 *
 * @param   aTestFile
 *          The test file object that describes the file to lock.
 */
function runHelperLockFile(aTestFile) {
  // Exclusively lock an existing file so it is in use during the update.
  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  let helperDestDir = getApplyDirFile(DIR_RESOURCES);
  helperBin.copyTo(helperDestDir, FILE_HELPER_BIN);
  helperBin = getApplyDirFile(DIR_RESOURCES + FILE_HELPER_BIN);
  // Strip off the first two directories so the path has to be from the helper's
  // working directory.
  let lockFileRelPath = aTestFile.relPathDir.split("/");
  if (IS_MACOSX) {
    lockFileRelPath = lockFileRelPath.slice(2);
  }
  lockFileRelPath = lockFileRelPath.join("/") + "/" + aTestFile.fileName;
  let args = [getApplyDirPath() + DIR_RESOURCES, "input", "output", "-s",
              HELPER_SLEEP_TIMEOUT, lockFileRelPath];
  let helperProcess = Cc["@mozilla.org/process/util;1"].
                        createInstance(Ci.nsIProcess);
  helperProcess.init(helperBin);
  helperProcess.run(false, args, args.length);

  do_execute_soon(waitForHelperSleep);
}

/**
 * Helper function that waits until the helper has completed its operations and
 * calls waitForHelperSleepFinished when it is finished.
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
    // Uses do_timeout instead of do_execute_soon to lessen log spew.
    do_timeout(FILE_IN_USE_TIMEOUT_MS, waitForHelperSleep);
    return;
  }
  try {
    output.remove(false);
  } catch (e) {
    if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
      do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the helper " +
               "message file to no longer be in use. Path: " + output.path);
    }
    debugDump("failed to remove file. Path: " + output.path);
    // Uses do_timeout instead of do_execute_soon to lessen log spew.
    do_timeout(FILE_IN_USE_TIMEOUT_MS, waitForHelperSleep);
    return;
  }
  waitForHelperSleepFinished();
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
    // Uses do_timeout instead of do_execute_soon to lessen log spew.
    do_timeout(FILE_IN_USE_TIMEOUT_MS, waitForHelperFinished);
    return;
  }
  // Give the lock file process time to unlock the file before deleting the
  // input and output files.
  waitForHelperFinishFileUnlock();
}

/**
 * Helper function that waits until the helper's input and output files are no
 * longer in use before calling waitForHelperExitFinished.
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
    do_execute_soon(waitForHelperFinishFileUnlock);
    return;
  }
  do_execute_soon(waitForHelperExitFinished);
}

/**
 * Helper function to tell the helper to finish and exit its sleep state.
 */
function waitForHelperExit() {
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
 * @param   aPostUpdateAsync
 *          When null the updater.ini is not created otherwise this parameter
 *          is passed to createUpdaterINI.
 * @param   aPostUpdateExeRelPathPrefix
 *          When aPostUpdateAsync null this value is ignored otherwise it is
 *          passed to createUpdaterINI.
 */
function setupUpdaterTest(aMarFile, aPostUpdateAsync,
                          aPostUpdateExeRelPathPrefix = "") {
  let updatesPatchDir = getUpdatesPatchDir();
  if (!updatesPatchDir.exists()) {
    updatesPatchDir.create(Ci.nsIFile.DIRECTORY_TYPE, PERMS_DIRECTORY);
  }
  // Copy the mar that will be applied
  let mar = getTestDirFile(aMarFile);
  mar.copyToFollowingLinks(updatesPatchDir, FILE_UPDATE_MAR);

  let helperBin = getTestDirFile(FILE_HELPER_BIN);
  helperBin.permissions = PERMS_DIRECTORY;
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

  setupActiveUpdate();

  if (aPostUpdateAsync !== null) {
    createUpdaterINI(aPostUpdateAsync, aPostUpdateExeRelPathPrefix);
  }

  setupAppFilesAsync();
}

/**
 * Helper function for updater binary tests that creates the update-settings.ini
 * file.
 */
function createUpdateSettingsINI() {
  let ini = getApplyDirFile(DIR_RESOURCES + FILE_UPDATE_SETTINGS_INI, true);
  writeFile(ini, UPDATE_SETTINGS_CONTENTS);
}

/**
 * Helper function for updater binary tests that creates the updater.ini
 * file.
 *
 * @param   aIsExeAsync
 *          True or undefined if the post update process should be async. If
 *          undefined ExeAsync will not be added to the updater.ini file in
 *          order to test the default launch behavior which is async.
 * @param   aExeRelPathPrefix
 *          A string to prefix the ExeRelPath values in the updater.ini.
 */
function createUpdaterINI(aIsExeAsync, aExeRelPathPrefix) {
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

  if (aExeRelPathPrefix && IS_WIN) {
    aExeRelPathPrefix = aExeRelPathPrefix.replace("/", "\\");
  }

  let exeRelPathMac = "ExeRelPath=" + aExeRelPathPrefix + DIR_RESOURCES +
                      gPostUpdateBinFile + "\n";
  let exeRelPathWin = "ExeRelPath=" + aExeRelPathPrefix + gPostUpdateBinFile + "\n";
  let updaterIniContents = "[Strings]\n" +
                           "Title=Update Test\n" +
                           "Info=Running update test " + gTestID + "\n\n" +
                           "[PostUpdateMac]\n" +
                           exeRelPathMac +
                           exeArg +
                           exeAsync +
                           "\n" +
                           "[PostUpdateWin]\n" +
                           exeRelPathWin +
                           exeArg +
                           exeAsync;
  let updaterIni = getApplyDirFile(DIR_RESOURCES + FILE_UPDATER_INI, true);
  writeFile(updaterIni, updaterIniContents);
}

/**
 * Gets the message log path used for assert checks to lessen the length printed
 * to the log file.
 *
 * @param   aPath
 *          The path to shorten for the log file.
 * @return  the message including the shortened path for the log file.
 */
function getMsgPath(aPath) {
  return ", path: " + replaceLogPaths(aPath);
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
 * @param   aStaged
 *          If the update log file is for a staged update.
 * @param   aReplace
 *          If the update log file is for a replace update.
 * @param   aExcludeDistDir
 *          Removes lines containing the distribution directory from the log
 *          file to compare the update log with.
 */
function checkUpdateLogContents(aCompareLogFile, aStaged = false,
                                aReplace = false, aExcludeDistDir = false) {
  if (IS_UNIX && !IS_MACOSX) {
    // The order that files are returned when enumerating the file system on
    // Linux is not deterministic so skip checking the logs.
    return;
  }

  let updateLog = getUpdateLog(FILE_LAST_UPDATE_LOG);
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

  // Skip the source/destination lines since they contain absolute paths.
  // These could be changed to relative paths using <test_dir_path> and
  // <update_dir_path>
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

  if (IS_WIN) {
    // The FindFile results when enumerating the filesystem on Windows is not
    // determistic so the results matching the following need to be fixed.
    let re = new RegExp("([^\n]* 7\/7text1[^\n]*)\n" +
                        "([^\n]* 7\/7text0[^\n]*)\n", "g");
    updateLogContents = updateLogContents.replace(re, "$2\n$1\n");
  }

  if (aReplace) {
    // Remove the lines which contain absolute paths
    updateLogContents = updateLogContents.replace(/^Begin moving.*$/mg, "");
    updateLogContents = updateLogContents.replace(/^ensure_remove: failed to remove file: .*$/mg, "");
    updateLogContents = updateLogContents.replace(/^ensure_remove_recursive: unable to remove directory: .*$/mg, "");
    updateLogContents = updateLogContents.replace(/^Removing tmpDir failed, err: -1$/mg, "");
    updateLogContents = updateLogContents.replace(/^remove_recursive_on_reboot: .*$/mg, "");
  }

  // Remove carriage returns.
  updateLogContents = updateLogContents.replace(/\r/g, "");
  // Replace error codes since they are different on each platform.
  updateLogContents = updateLogContents.replace(/, err:.*\n/g, "\n");
  // Replace to make the log parsing happy.
  updateLogContents = updateLogContents.replace(/non-fatal error /g, "");
  // Remove consecutive newlines
  updateLogContents = updateLogContents.replace(/\n+/g, "\n");
  // Remove leading and trailing newlines
  updateLogContents = updateLogContents.replace(/^\n|\n$/g, "");
  // Replace the log paths with <test_dir_path> and <update_dir_path>
  updateLogContents = replaceLogPaths(updateLogContents);

  let compareLogContents = "";
  if (aCompareLogFile) {
    compareLogContents = readFileBytes(getTestDirFile(aCompareLogFile));
  }

  if (aStaged) {
    compareLogContents = PERFORMING_STAGED_UPDATE + "\n" + compareLogContents;
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

  if (aExcludeDistDir) {
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
    logUpdateLog(FILE_LAST_UPDATE_LOG);
    let aryLog = updateLogContents.split("\n");
    let aryCompare = compareLogContents.split("\n");
    // Pushing an empty string to both arrays makes it so either array's length
    // can be used in the for loop below without going out of bounds.
    aryLog.push("");
    aryCompare.push("");
    // xpcshell tests won't display the entire contents so log the first
    // incorrect line.
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
  let updateLog = getUpdateLog(FILE_LAST_UPDATE_LOG);
  let updateLogContents = readFileBytes(updateLog).replace(/\r\n/g, "\n");
  updateLogContents = replaceLogPaths(updateLogContents);
  Assert.notEqual(updateLogContents.indexOf(aCheckString), -1,
                  "the update log contents should contain value: " +
                  aCheckString);
}

/**
 * Helper function for updater binary tests for verifying the state of files and
 * directories after a successful update.
 *
 * @param   aGetFileFunc
 *          The function used to get the files in the directory to be checked.
 * @param   aStageDirExists
 *          If true the staging directory will be tested for existence and if
 *          false the staging directory will be tested for non-existence.
 * @param   aToBeDeletedDirExists
 *          On Windows, if true the tobedeleted directory will be tested for
 *          existence and if false the tobedeleted directory will be tested for
 *          non-existence. On all othere platforms it will be tested for
 *          non-existence.
 */
function checkFilesAfterUpdateSuccess(aGetFileFunc, aStageDirExists = false,
                                      aToBeDeletedDirExists = false) {
  debugDump("testing contents of files after a successful update");
  gTestFiles.forEach(function CFAUS_TF_FE(aTestFile) {
    let testFile = aGetFileFunc(aTestFile.relPathDir + aTestFile.fileName, true);
    debugDump("testing file: " + testFile.path);
    if (aTestFile.compareFile || aTestFile.compareContents) {
      Assert.ok(testFile.exists(),
                MSG_SHOULD_EXIST + getMsgPath(testFile.path));

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
      Assert.ok(!testFile.exists(),
                MSG_SHOULD_NOT_EXIST + getMsgPath(testFile.path));
    }
  });

  debugDump("testing operations specified in removed-files were performed " +
            "after a successful update");
  gTestDirs.forEach(function CFAUS_TD_FE(aTestDir) {
    let testDir = aGetFileFunc(aTestDir.relPathDir, true);
    debugDump("testing directory: " + testDir.path);
    if (aTestDir.dirRemoved) {
      Assert.ok(!testDir.exists(),
                MSG_SHOULD_NOT_EXIST + getMsgPath(testDir.path));
    } else {
      Assert.ok(testDir.exists(),
                MSG_SHOULD_EXIST + getMsgPath(testDir.path));

      if (aTestDir.files) {
        aTestDir.files.forEach(function CFAUS_TD_F_FE(aTestFile) {
          let testFile = aGetFileFunc(aTestDir.relPathDir + aTestFile, true);
          if (aTestDir.filesRemoved) {
            Assert.ok(!testFile.exists(),
                      MSG_SHOULD_NOT_EXIST + getMsgPath(testFile.path));
          } else {
            Assert.ok(testFile.exists(),
                      MSG_SHOULD_EXIST + getMsgPath(testFile.path));
          }
        });
      }

      if (aTestDir.subDirs) {
        aTestDir.subDirs.forEach(function CFAUS_TD_SD_FE(aSubDir) {
          let testSubDir = aGetFileFunc(aTestDir.relPathDir + aSubDir, true);
          Assert.ok(testSubDir.exists(),
                    MSG_SHOULD_EXIST + getMsgPath(testSubDir.path));
          if (aTestDir.subDirFiles) {
            aTestDir.subDirFiles.forEach(function CFAUS_TD_SDF_FE(aTestFile) {
              let testFile = aGetFileFunc(aTestDir.relPathDir +
                                          aSubDir + aTestFile, true);
              Assert.ok(testFile.exists(),
                        MSG_SHOULD_EXIST + getMsgPath(testFile.path));
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
 * @param   aGetFileFunc
 *          The function used to get the files in the directory to be checked.
 * @param   aStageDirExists
 *          If true the staging directory will be tested for existence and if
 *          false the staging directory will be tested for non-existence.
 * @param   aToBeDeletedDirExists
 *          On Windows, if true the tobedeleted directory will be tested for
 *          existence and if false the tobedeleted directory will be tested for
 *          non-existence. On all othere platforms it will be tested for
 *          non-existence.
 */
function checkFilesAfterUpdateFailure(aGetFileFunc, aStageDirExists = false,
                                      aToBeDeletedDirExists = false) {
  debugDump("testing contents of files after a failed update");
  gTestFiles.forEach(function CFAUF_TF_FE(aTestFile) {
    let testFile = aGetFileFunc(aTestFile.relPathDir + aTestFile.fileName, true);
    debugDump("testing file: " + testFile.path);
    if (aTestFile.compareFile || aTestFile.compareContents) {
      Assert.ok(testFile.exists(),
                MSG_SHOULD_EXIST + getMsgPath(testFile.path));

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
      Assert.ok(!testFile.exists(),
                MSG_SHOULD_NOT_EXIST + getMsgPath(testFile.path));
    }
  });

  debugDump("testing operations specified in removed-files were not " +
            "performed after a failed update");
  gTestDirs.forEach(function CFAUF_TD_FE(aTestDir) {
    let testDir = aGetFileFunc(aTestDir.relPathDir, true);
    Assert.ok(testDir.exists(),
              MSG_SHOULD_EXIST + getMsgPath(testDir.path));

    if (aTestDir.files) {
      aTestDir.files.forEach(function CFAUS_TD_F_FE(aTestFile) {
        let testFile = aGetFileFunc(aTestDir.relPathDir + aTestFile, true);
        Assert.ok(testFile.exists(),
                  MSG_SHOULD_EXIST + getMsgPath(testFile.path));
      });
    }

    if (aTestDir.subDirs) {
      aTestDir.subDirs.forEach(function CFAUS_TD_SD_FE(aSubDir) {
        let testSubDir = aGetFileFunc(aTestDir.relPathDir + aSubDir, true);
        Assert.ok(testSubDir.exists(),
                  MSG_SHOULD_EXIST + getMsgPath(testSubDir.path));
        if (aTestDir.subDirFiles) {
          aTestDir.subDirFiles.forEach(function CFAUS_TD_SDF_FE(aTestFile) {
            let testFile = aGetFileFunc(aTestDir.relPathDir +
                                        aSubDir + aTestFile, true);
            Assert.ok(testFile.exists(),
                      MSG_SHOULD_EXIST + getMsgPath(testFile.path));
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
 * @param   aGetFileFunc
 *          the function used to get the files in the directory to be checked.
 * @param   aStageDirExists
 *          If true the staging directory will be tested for existence and if
 *          false the staging directory will be tested for non-existence.
 * @param   aToBeDeletedDirExists
 *          On Windows, if true the tobedeleted directory will be tested for
 *          existence and if false the tobedeleted directory will be tested for
 *          non-existence. On all othere platforms it will be tested for
 *          non-existence.
 */
function checkFilesAfterUpdateCommon(aGetFileFunc, aStageDirExists,
                                     aToBeDeletedDirExists) {
  debugDump("testing extra directories");
  let stageDir = getStageDirFile(null, true);
  if (aStageDirExists) {
    Assert.ok(stageDir.exists(),
              MSG_SHOULD_EXIST + getMsgPath(stageDir.path));
  } else {
    Assert.ok(!stageDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(stageDir.path));
  }

  let toBeDeletedDirExists = IS_WIN ? aToBeDeletedDirExists : false;
  let toBeDeletedDir = getApplyDirFile(DIR_TOBEDELETED, true);
  if (toBeDeletedDirExists) {
    Assert.ok(toBeDeletedDir.exists(),
              MSG_SHOULD_EXIST + getMsgPath(toBeDeletedDir.path));
  } else {
    Assert.ok(!toBeDeletedDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(toBeDeletedDir.path));
  }

  let updatingDir = getApplyDirFile("updating", true);
  Assert.ok(!updatingDir.exists(),
            MSG_SHOULD_NOT_EXIST + getMsgPath(updatingDir.path));

  if (stageDir.exists()) {
    updatingDir = stageDir.clone();
    updatingDir.append("updating");
    Assert.ok(!updatingDir.exists(),
              MSG_SHOULD_NOT_EXIST + getMsgPath(updatingDir.path));
  }

  debugDump("testing backup files should not be left behind in the " +
            "application directory");
  let applyToDir = getApplyDirFile(null, true);
  checkFilesInDirRecursive(applyToDir, checkForBackupFiles);

  if (stageDir.exists()) {
    debugDump("testing backup files should not be left behind in the " +
              "staging directory");
    applyToDir = getApplyDirFile(null, true);
    checkFilesInDirRecursive(stageDir, checkForBackupFiles);
  }
}

/**
 * Helper function for updater binary tests for verifying the contents of the
 * updater callback application log which should contain the arguments passed to
 * the callback application.
 */
function checkCallbackLog() {
  let appLaunchLog = getApplyDirFile(DIR_RESOURCES + gCallbackArgs[1], true);
  if (!appLaunchLog.exists()) {
    // Uses do_timeout instead of do_execute_soon to lessen log spew.
    do_timeout(FILE_IN_USE_TIMEOUT_MS, checkCallbackLog);
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
    // Uses do_timeout instead of do_execute_soon to lessen log spew.
    do_timeout(FILE_IN_USE_TIMEOUT_MS, checkCallbackLog);
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
 * Checks the contents of the updater post update binary log. When completed
 * checkPostUpdateAppLogFinished will be called.
 */
function checkPostUpdateAppLog() {
  // Only Mac OS X and Windows support post update.
  if (IS_MACOSX || IS_WIN) {
    gTimeoutRuns++;
    let postUpdateLog = getPostUpdateFile(".log");
    if (!postUpdateLog.exists()) {
      debugDump("postUpdateLog does not exist. Path: " + postUpdateLog.path);
      if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
        do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the post update " +
                 "process to create the post update log. Path: " +
                 postUpdateLog.path);
      }
      do_execute_soon(checkPostUpdateAppLog);
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
      do_execute_soon(checkPostUpdateAppLog);
      return;
    }
    Assert.ok(true, "the post update log contents" + MSG_SHOULD_EQUAL);
  }

  do_execute_soon(checkPostUpdateAppLogFinished);
}

/**
 * Helper function to check if a file is in use on Windows by making a copy of
 * a file and attempting to delete the original file. If the deletion is
 * successful the copy of the original file is renamed to the original file's
 * name and if the deletion is not successful the copy of the original file is
 * deleted.
 *
 * @param   aFile
 *          An nsIFile for the file to be checked if it is in use.
 * @return  true if the file can't be deleted and false otherwise.
 */
function isFileInUse(aFile) {
  if (!IS_WIN) {
    do_throw("Windows only function called by a different platform!");
  }

  if (!aFile.exists()) {
    debugDump("file does not exist, path: " + aFile.path);
    return false;
  }

  let fileBak = aFile.parent;
  fileBak.append(aFile.leafName + ".bak");
  try {
    if (fileBak.exists()) {
      fileBak.remove(false);
    }
    aFile.copyTo(aFile.parent, fileBak.leafName);
    aFile.remove(false);
    fileBak.moveTo(aFile.parent, aFile.leafName);
    debugDump("file is not in use, path: " + aFile.path);
    return false;
  } catch (e) {
    debugDump("file in use, path: " + aFile.path + ", exception: " + e);
    try {
      if (fileBak.exists()) {
        fileBak.remove(false);
      }
    } catch (ex) {
      logTestInfo("unable to remove backup file, path: " +
                  fileBak.path + ", exception: " + ex);
    }
  }
  return true;
}

/**
 * Waits until files that are in use that break tests are no longer in use and
 * then calls doTestFinish to end the test.
 */
function waitForFilesInUse() {
  if (IS_WIN) {
    let fileNames = [FILE_APP_BIN, FILE_UPDATER_BIN,
                     FILE_MAINTENANCE_SERVICE_INSTALLER_BIN];
    for (let i = 0; i < fileNames.length; ++i) {
      let file = getApplyDirFile(fileNames[i], true);
      if (isFileInUse(file)) {
        do_timeout(FILE_IN_USE_TIMEOUT_MS, waitForFilesInUse);
        return;
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
                  "the file's extension should not equal moz-backup" +
                  getMsgPath(aFile.path));
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

    if (entry.exists()) {
      if (entry.isDirectory()) {
        checkFilesInDirRecursive(entry, aCallback);
      } else {
        aCallback(entry);
      }
    }
  }
}


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
    };
  });
}

UpdatePrompt.prototype = {
  flags: Ci.nsIClassInfo.SINGLETON,
  getScriptableHelper: () => null,
  getInterfaces(aCount) {
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
    if (gStatusCode == 0) {
      gStatusCode = aRequest.channel.QueryInterface(Ci.nsIRequest).status;
    }
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
  gTestserver.registerPathHandler("/" + gHTTPHandlerPath, pathHandler);
  gTestserver.start(-1);
  let testserverPort = gTestserver.identity.primaryPort;
  gURLData = URL_HOST + ":" + testserverPort + "/";
  debugDump("http server port = " + testserverPort);
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
    createInstance(aOuter, aIID) {
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
  let appArgsLog = do_get_file("/" + gTestID + "_app_args_log", true);
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
  let launchScript = do_get_file("/" + gTestID + "_launch.sh", true);
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
          return getApplyDirFile(DIR_RESOURCES, true);
        case NS_GRE_BIN_DIR:
          return getApplyDirFile(DIR_MACOS, true);
        case XRE_EXECUTABLE_FILE:
          return getApplyDirFile(DIR_MACOS + FILE_APP_BIN, true);
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

    if (gPIDPersistProcess && gPIDPersistProcess.isRunning) {
      debugDump("start - kill pid persist process");
      try {
        gPIDPersistProcess.kill();
      } catch (e) {
        debugDump("kill pid persist process failed. Exception: " + e);
      }
      gPIDPersistProcess = null;
      debugDump("finish - kill pid persist process");
    }

    if (gHandle) {
      try {
        debugDump("start - closing handle");
        let kernel32 = ctypes.open("kernel32");
        let CloseHandle = kernel32.declare("CloseHandle", ctypes.winapi_abi,
                                           ctypes.bool, /* return*/
                                           ctypes.voidptr_t /* handle*/);
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
    if (typeof end_test == typeof Function) {
      debugDump("calling end_test");
      end_test();
    }

    ds.unregisterProvider(dirProvider);
    cleanupTestCommon();

    debugDump("finish - unregistering directory provider");
  });
}

/**
 * The timer callback to kill the process if it takes too long.
 */
const gAppTimerCallback = {
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
 * Launches an application to apply an update.
 */
function runUpdateUsingApp(aExpectedStatus) {
  /**
   * The observer for the call to nsIProcess:runAsync. When completed
   * runUpdateFinished will be called.
   */
  const processObserver = {
    observe: function PO_observe(aSubject, aTopic, aData) {
      debugDump("topic: " + aTopic + ", process exitValue: " +
                gProcess.exitValue);
      resetEnvironment();
      if (gAppTimer) {
        gAppTimer.cancel();
        gAppTimer = null;
      }
      Assert.equal(gProcess.exitValue, 0,
                   "the application process exit value should be 0");
      Assert.equal(aTopic, "process-finished",
                   "the application process observer topic should be " +
                   "process-finished");

      if (IS_SERVICE_TEST) {
        waitForServiceStop(false);
      }

      do_execute_soon(afterAppExits);
    },
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver])
  };

  function afterAppExits() {
    gTimeoutRuns++;

    if (IS_WIN) {
      waitForApplicationStop(FILE_UPDATER_BIN);
    }

    let status;
    try {
      status = readStatusFile();
    } catch (e) {
      logTestInfo("error reading status file, exception: " + e);
    }
    // Don't proceed until the update's status is the expected value.
    if (status != aExpectedStatus) {
      if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
        logUpdateLog(FILE_UPDATE_LOG);
        do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
                 "status to equal: " +
                 aExpectedStatus +
                 ", current status: " + status);
      } else {
        do_timeout(FILE_IN_USE_TIMEOUT_MS, afterAppExits);
      }
      return;
    }

    // Don't check for an update log when the code in nsUpdateDriver.cpp skips
    // updating.
    if (aExpectedStatus != STATE_PENDING &&
        aExpectedStatus != STATE_PENDING_SVC &&
        aExpectedStatus != STATE_APPLIED &&
        aExpectedStatus != STATE_APPLIED_SVC) {
      // Don't proceed until the update log has been created.
      let log = getUpdateLog(FILE_UPDATE_LOG);
      if (!log.exists()) {
        if (gTimeoutRuns > MAX_TIMEOUT_RUNS) {
          do_throw("Exceeded MAX_TIMEOUT_RUNS while waiting for the update " +
                   "log to be created. Path: " + log.path);
        }
        do_timeout(FILE_IN_USE_TIMEOUT_MS, afterAppExits);
        return;
      }
    }

    do_execute_soon(runUpdateFinished);
  }

  debugDump("start - launching application to apply update");

  let launchBin = getLaunchBin();
  let args = getProcessArgs();
  debugDump("launching " + launchBin.path + " " + args.join(" "));

  gProcess = Cc["@mozilla.org/process/util;1"].
             createInstance(Ci.nsIProcess);
  gProcess.init(launchBin);

  gAppTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  gAppTimer.initWithCallback(gAppTimerCallback, APP_TIMER_TIMEOUT,
                             Ci.nsITimer.TYPE_ONE_SHOT);

  setEnvironment();
  debugDump("launching application");
  gProcess.runAsync(args, args.length, processObserver);

  debugDump("finish - launching application to apply update");
}

/**
 * Sets the environment that will be used by the application process when it is
 * launched.
 */
function setEnvironment() {
  if (IS_WIN) {
    // The tests use nsIProcess to launch the updater and it is simpler to just
    // set an environment variable and have the test updater set the current
    // working directory than it is to set the current working directory in the
    // test itself.
    gEnv.set("CURWORKDIRPATH", getApplyDirFile().path);
  }

  // Prevent setting the environment more than once.
  if (gShouldResetEnv !== undefined) {
    return;
  }

  gShouldResetEnv = true;

  // See bug 1279108.
  if (gEnv.exists("ASAN_OPTIONS")) {
    gASanOptions = gEnv.get("ASAN_OPTIONS");
    gEnv.set("ASAN_OPTIONS", gASanOptions + ":detect_leaks=0");
  } else {
    gEnv.set("ASAN_OPTIONS", "detect_leaks=0");
  }

  if (IS_WIN && !gEnv.exists("XRE_NO_WINDOWS_CRASH_DIALOG")) {
    gAddedEnvXRENoWindowsCrashDialog = true;
    debugDump("setting the XRE_NO_WINDOWS_CRASH_DIALOG environment " +
              "variable to 1... previously it didn't exist");
    gEnv.set("XRE_NO_WINDOWS_CRASH_DIALOG", "1");
  }

  if (IS_UNIX) {
    let appGreBinDir = gGREBinDirOrig.clone();
    let envGreBinDir = Cc["@mozilla.org/file/local;1"].
                       createInstance(Ci.nsILocalFile);
    let shouldSetEnv = true;
    if (IS_MACOSX) {
      if (gEnv.exists("DYLD_LIBRARY_PATH")) {
        gEnvDyldLibraryPath = gEnv.get("DYLD_LIBRARY_PATH");
        envGreBinDir.initWithPath(gEnvDyldLibraryPath);
        if (envGreBinDir.path == appGreBinDir.path) {
          gEnvDyldLibraryPath = null;
          shouldSetEnv = false;
        }
      }

      if (shouldSetEnv) {
        debugDump("setting DYLD_LIBRARY_PATH environment variable value to " +
                  appGreBinDir.path);
        gEnv.set("DYLD_LIBRARY_PATH", appGreBinDir.path);
      }
    } else {
      if (gEnv.exists("LD_LIBRARY_PATH")) {
        gEnvLdLibraryPath = gEnv.get("LD_LIBRARY_PATH");
        envGreBinDir.initWithPath(gEnvLdLibraryPath);
        if (envGreBinDir.path == appGreBinDir.path) {
          gEnvLdLibraryPath = null;
          shouldSetEnv = false;
        }
      }

      if (shouldSetEnv) {
        debugDump("setting LD_LIBRARY_PATH environment variable value to " +
                  appGreBinDir.path);
        gEnv.set("LD_LIBRARY_PATH", appGreBinDir.path);
      }
    }
  }

  if (gEnv.exists("XPCOM_MEM_LEAK_LOG")) {
    gEnvXPCOMMemLeakLog = gEnv.get("XPCOM_MEM_LEAK_LOG");
    debugDump("removing the XPCOM_MEM_LEAK_LOG environment variable... " +
              "previous value " + gEnvXPCOMMemLeakLog);
    gEnv.set("XPCOM_MEM_LEAK_LOG", "");
  }

  if (gEnv.exists("XPCOM_DEBUG_BREAK")) {
    gEnvXPCOMDebugBreak = gEnv.get("XPCOM_DEBUG_BREAK");
    debugDump("setting the XPCOM_DEBUG_BREAK environment variable to " +
              "warn... previous value " + gEnvXPCOMDebugBreak);
  } else {
    debugDump("setting the XPCOM_DEBUG_BREAK environment variable to " +
              "warn... previously it didn't exist");
  }

  gEnv.set("XPCOM_DEBUG_BREAK", "warn");

  if (IS_SERVICE_TEST) {
    debugDump("setting MOZ_NO_SERVICE_FALLBACK environment variable to 1");
    gEnv.set("MOZ_NO_SERVICE_FALLBACK", "1");
  }
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

  // Restore previous ASAN_OPTIONS if there were any.
  gEnv.set("ASAN_OPTIONS", gASanOptions ? gASanOptions : "");

  if (gEnvXPCOMMemLeakLog) {
    debugDump("setting the XPCOM_MEM_LEAK_LOG environment variable back to " +
              gEnvXPCOMMemLeakLog);
    gEnv.set("XPCOM_MEM_LEAK_LOG", gEnvXPCOMMemLeakLog);
  }

  if (gEnvXPCOMDebugBreak) {
    debugDump("setting the XPCOM_DEBUG_BREAK environment variable back to " +
              gEnvXPCOMDebugBreak);
    gEnv.set("XPCOM_DEBUG_BREAK", gEnvXPCOMDebugBreak);
  } else if (gEnv.exists("XPCOM_DEBUG_BREAK")) {
    debugDump("clearing the XPCOM_DEBUG_BREAK environment variable");
    gEnv.set("XPCOM_DEBUG_BREAK", "");
  }

  if (IS_UNIX) {
    if (IS_MACOSX) {
      if (gEnvDyldLibraryPath) {
        debugDump("setting DYLD_LIBRARY_PATH environment variable value " +
                  "back to " + gEnvDyldLibraryPath);
        gEnv.set("DYLD_LIBRARY_PATH", gEnvDyldLibraryPath);
      } else if (gEnvDyldLibraryPath !== null) {
        debugDump("removing DYLD_LIBRARY_PATH environment variable");
        gEnv.set("DYLD_LIBRARY_PATH", "");
      }
    } else if (gEnvLdLibraryPath) {
      debugDump("setting LD_LIBRARY_PATH environment variable value back " +
                "to " + gEnvLdLibraryPath);
      gEnv.set("LD_LIBRARY_PATH", gEnvLdLibraryPath);
    } else if (gEnvLdLibraryPath !== null) {
      debugDump("removing LD_LIBRARY_PATH environment variable");
      gEnv.set("LD_LIBRARY_PATH", "");
    }
  }

  if (IS_WIN && gAddedEnvXRENoWindowsCrashDialog) {
    debugDump("removing the XRE_NO_WINDOWS_CRASH_DIALOG environment " +
              "variable");
    gEnv.set("XRE_NO_WINDOWS_CRASH_DIALOG", "");
  }

  if (IS_SERVICE_TEST) {
    debugDump("removing MOZ_NO_SERVICE_FALLBACK environment variable");
    gEnv.set("MOZ_NO_SERVICE_FALLBACK", "");
  }
}
