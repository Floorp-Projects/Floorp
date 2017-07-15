/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/FileUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/ctypes.jsm", this);
Cu.import("resource://gre/modules/UpdateTelemetry.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm", this);
Cu.importGlobalProperties(["XMLHttpRequest"]);

const UPDATESERVICE_CID = Components.ID("{B3C290A6-3943-4B89-8BBE-C01EB7B3B311}");
const UPDATESERVICE_CONTRACTID = "@mozilla.org/updates/update-service;1";

const PREF_APP_UPDATE_ALTWINDOWTYPE        = "app.update.altwindowtype";
const PREF_APP_UPDATE_AUTO                 = "app.update.auto";
const PREF_APP_UPDATE_BACKGROUNDINTERVAL   = "app.update.download.backgroundInterval";
const PREF_APP_UPDATE_BACKGROUNDERRORS     = "app.update.backgroundErrors";
const PREF_APP_UPDATE_BACKGROUNDMAXERRORS  = "app.update.backgroundMaxErrors";
const PREF_APP_UPDATE_CANCELATIONS         = "app.update.cancelations";
const PREF_APP_UPDATE_CANCELATIONS_OSX     = "app.update.cancelations.osx";
const PREF_APP_UPDATE_CANCELATIONS_OSX_MAX = "app.update.cancelations.osx.max";
const PREF_APP_UPDATE_DOORHANGER           = "app.update.doorhanger";
const PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS    = "app.update.download.attempts";
const PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS = "app.update.download.maxAttempts";
const PREF_APP_UPDATE_ELEVATE_NEVER        = "app.update.elevate.never";
const PREF_APP_UPDATE_ELEVATE_VERSION      = "app.update.elevate.version";
const PREF_APP_UPDATE_ELEVATE_ATTEMPTS     = "app.update.elevate.attempts";
const PREF_APP_UPDATE_ELEVATE_MAXATTEMPTS  = "app.update.elevate.maxAttempts";
const PREF_APP_UPDATE_ENABLED              = "app.update.enabled";
const PREF_APP_UPDATE_IDLETIME             = "app.update.idletime";
const PREF_APP_UPDATE_LOG                  = "app.update.log";
const PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED  = "app.update.notifiedUnsupported";
const PREF_APP_UPDATE_POSTUPDATE           = "app.update.postupdate";
const PREF_APP_UPDATE_PROMPTWAITTIME       = "app.update.promptWaitTime";
const PREF_APP_UPDATE_SERVICE_ENABLED      = "app.update.service.enabled";
const PREF_APP_UPDATE_SERVICE_ERRORS       = "app.update.service.errors";
const PREF_APP_UPDATE_SERVICE_MAXERRORS    = "app.update.service.maxErrors";
const PREF_APP_UPDATE_SILENT               = "app.update.silent";
const PREF_APP_UPDATE_SOCKET_MAXERRORS     = "app.update.socket.maxErrors";
const PREF_APP_UPDATE_SOCKET_RETRYTIMEOUT  = "app.update.socket.retryTimeout";
const PREF_APP_UPDATE_STAGING_ENABLED      = "app.update.staging.enabled";
const PREF_APP_UPDATE_URL                  = "app.update.url";
const PREF_APP_UPDATE_URL_DETAILS          = "app.update.url.details";

const URI_BRAND_PROPERTIES      = "chrome://branding/locale/brand.properties";
const URI_UPDATE_HISTORY_DIALOG = "chrome://mozapps/content/update/history.xul";
const URI_UPDATE_NS             = "http://www.mozilla.org/2005/app-update";
const URI_UPDATE_PROMPT_DIALOG  = "chrome://mozapps/content/update/updates.xul";
const URI_UPDATES_PROPERTIES    = "chrome://mozapps/locale/update/updates.properties";

const KEY_UPDROOT         = "UpdRootD";
const KEY_EXECUTABLE      = "XREExeF";

const DIR_UPDATES         = "updates";

const FILE_ACTIVE_UPDATE_XML = "active-update.xml";
const FILE_BACKUP_UPDATE_LOG = "backup-update.log";
const FILE_LAST_UPDATE_LOG   = "last-update.log";
const FILE_UPDATES_XML       = "updates.xml";
const FILE_UPDATE_LOG        = "update.log";
const FILE_UPDATE_MAR        = "update.mar";
const FILE_UPDATE_STATUS     = "update.status";
const FILE_UPDATE_TEST       = "update.test";
const FILE_UPDATE_VERSION    = "update.version";

const STATE_NONE            = "null";
const STATE_DOWNLOADING     = "downloading";
const STATE_PENDING         = "pending";
const STATE_PENDING_SERVICE = "pending-service";
const STATE_PENDING_ELEVATE = "pending-elevate";
const STATE_APPLYING        = "applying";
const STATE_APPLIED         = "applied";
const STATE_APPLIED_SERVICE = "applied-service";
const STATE_SUCCEEDED       = "succeeded";
const STATE_DOWNLOAD_FAILED = "download-failed";
const STATE_FAILED          = "failed";

// The values below used by this code are from common/errors.h
const WRITE_ERROR                          = 7;
const ELEVATION_CANCELED                   = 9;
const SERVICE_UPDATER_COULD_NOT_BE_STARTED = 24;
const SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS = 25;
const SERVICE_UPDATER_SIGN_ERROR           = 26;
const SERVICE_UPDATER_COMPARE_ERROR        = 27;
const SERVICE_UPDATER_IDENTITY_ERROR       = 28;
const SERVICE_STILL_APPLYING_ON_SUCCESS    = 29;
const SERVICE_STILL_APPLYING_ON_FAILURE    = 30;
const SERVICE_UPDATER_NOT_FIXED_DRIVE      = 31;
const SERVICE_COULD_NOT_LOCK_UPDATER       = 32;
const SERVICE_INSTALLDIR_ERROR             = 33;
const WRITE_ERROR_ACCESS_DENIED            = 35;
const WRITE_ERROR_CALLBACK_APP             = 37;
const SERVICE_COULD_NOT_COPY_UPDATER       = 49;
const SERVICE_STILL_APPLYING_TERMINATED    = 50;
const SERVICE_STILL_APPLYING_NO_EXIT_CODE  = 51;
const WRITE_ERROR_FILE_COPY                = 61;
const WRITE_ERROR_DELETE_FILE              = 62;
const WRITE_ERROR_OPEN_PATCH_FILE          = 63;
const WRITE_ERROR_PATCH_FILE               = 64;
const WRITE_ERROR_APPLY_DIR_PATH           = 65;
const WRITE_ERROR_CALLBACK_PATH            = 66;
const WRITE_ERROR_FILE_ACCESS_DENIED       = 67;
const WRITE_ERROR_DIR_ACCESS_DENIED        = 68;
const WRITE_ERROR_DELETE_BACKUP            = 69;
const WRITE_ERROR_EXTRACT                  = 70;

// Array of write errors to simplify checks for write errors
const WRITE_ERRORS = [WRITE_ERROR,
                      WRITE_ERROR_ACCESS_DENIED,
                      WRITE_ERROR_CALLBACK_APP,
                      WRITE_ERROR_FILE_COPY,
                      WRITE_ERROR_DELETE_FILE,
                      WRITE_ERROR_OPEN_PATCH_FILE,
                      WRITE_ERROR_PATCH_FILE,
                      WRITE_ERROR_APPLY_DIR_PATH,
                      WRITE_ERROR_CALLBACK_PATH,
                      WRITE_ERROR_FILE_ACCESS_DENIED,
                      WRITE_ERROR_DIR_ACCESS_DENIED,
                      WRITE_ERROR_DELETE_BACKUP,
                      WRITE_ERROR_EXTRACT];

// Array of write errors to simplify checks for service errors
const SERVICE_ERRORS = [SERVICE_UPDATER_COULD_NOT_BE_STARTED,
                        SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS,
                        SERVICE_UPDATER_SIGN_ERROR,
                        SERVICE_UPDATER_COMPARE_ERROR,
                        SERVICE_UPDATER_IDENTITY_ERROR,
                        SERVICE_STILL_APPLYING_ON_SUCCESS,
                        SERVICE_STILL_APPLYING_ON_FAILURE,
                        SERVICE_UPDATER_NOT_FIXED_DRIVE,
                        SERVICE_COULD_NOT_LOCK_UPDATER,
                        SERVICE_INSTALLDIR_ERROR,
                        SERVICE_COULD_NOT_COPY_UPDATER,
                        SERVICE_STILL_APPLYING_TERMINATED,
                        SERVICE_STILL_APPLYING_NO_EXIT_CODE];

// Error codes 80 through 99 are reserved for nsUpdateService.js and are not
// defined in common/errors.h
const INVALID_UPDATER_STATE_CODE           = 98;
const INVALID_UPDATER_STATUS_CODE          = 99;

// Custom update error codes
const BACKGROUNDCHECK_MULTIPLE_FAILURES = 110;
const NETWORK_ERROR_OFFLINE             = 111;

// Error codes should be < 1000. Errors above 1000 represent http status codes
const HTTP_ERROR_OFFSET                 = 1000;

const DOWNLOAD_CHUNK_SIZE           = 300000; // bytes
const DOWNLOAD_BACKGROUND_INTERVAL  = 600;    // seconds
const DOWNLOAD_FOREGROUND_INTERVAL  = 0;

const UPDATE_WINDOW_NAME      = "Update:Wizard";

// The number of consecutive failures when updating using the service before
// setting the app.update.service.enabled preference to false.
const DEFAULT_SERVICE_MAX_ERRORS = 10;

// The number of consecutive socket errors to allow before falling back to
// downloading a different MAR file or failing if already downloading the full.
const DEFAULT_SOCKET_MAX_ERRORS = 10;

// The number of milliseconds to wait before retrying a connection error.
const DEFAULT_SOCKET_RETRYTIMEOUT = 2000;

// Default maximum number of elevation cancelations per update version before
// giving up.
const DEFAULT_CANCELATIONS_OSX_MAX = 3;

// This maps app IDs to their respective notification topic which signals when
// the application's user interface has been displayed.
const APPID_TO_TOPIC = {
  // Firefox
  "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "sessionstore-windows-restored",
  // SeaMonkey
  "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "sessionstore-windows-restored",
  // Thunderbird
  "{3550f703-e582-4d05-9a08-453d09bdfdc6}": "mail-startup-done",
  // Instantbird
  "{33cb9019-c295-46dd-be21-8c4936574bee}": "xul-window-visible",
};

var gUpdateMutexHandle = null;

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "gLogEnabled", function aus_gLogEnabled() {
  return getPref("getBoolPref", PREF_APP_UPDATE_LOG, false);
});

XPCOMUtils.defineLazyGetter(this, "gUpdateBundle", function aus_gUpdateBundle() {
  return Services.strings.createBundle(URI_UPDATES_PROPERTIES);
});

// shared code for suppressing bad cert dialogs
XPCOMUtils.defineLazyGetter(this, "gCertUtils", function aus_gCertUtils() {
  let temp = { };
  Cu.import("resource://gre/modules/CertUtils.jsm", temp);
  return temp;
});

/**
 * Tests to make sure that we can write to a given directory.
 *
 * @param updateTestFile a test file in the directory that needs to be tested.
 * @param createDirectory whether a test directory should be created.
 * @throws if we don't have right access to the directory.
 */
function testWriteAccess(updateTestFile, createDirectory) {
  const NORMAL_FILE_TYPE = Ci.nsILocalFile.NORMAL_FILE_TYPE;
  const DIRECTORY_TYPE = Ci.nsILocalFile.DIRECTORY_TYPE;
  if (updateTestFile.exists())
    updateTestFile.remove(false);
  updateTestFile.create(createDirectory ? DIRECTORY_TYPE : NORMAL_FILE_TYPE,
                        createDirectory ? FileUtils.PERMS_DIRECTORY : FileUtils.PERMS_FILE);
  updateTestFile.remove(false);
}

/**
 * Windows only function that closes a Win32 handle.
 *
 * @param handle The handle to close
 */
function closeHandle(handle) {
  let lib = ctypes.open("kernel32.dll");
  let CloseHandle = lib.declare("CloseHandle",
                                ctypes.winapi_abi,
                                ctypes.int32_t, /* success */
                                ctypes.void_t.ptr); /* handle */
  CloseHandle(handle);
  lib.close();
}

/**
 * Windows only function that creates a mutex.
 *
 * @param  aName
 *         The name for the mutex.
 * @param  aAllowExisting
 *         If false the function will close the handle and return null.
 * @return The Win32 handle to the mutex.
 */
function createMutex(aName, aAllowExisting = true) {
  if (AppConstants.platform != "win") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  const INITIAL_OWN = 1;
  const ERROR_ALREADY_EXISTS = 0xB7;
  let lib = ctypes.open("kernel32.dll");
  let CreateMutexW = lib.declare("CreateMutexW",
                                 ctypes.winapi_abi,
                                 ctypes.void_t.ptr, /* return handle */
                                 ctypes.void_t.ptr, /* security attributes */
                                 ctypes.int32_t, /* initial owner */
                                 ctypes.char16_t.ptr); /* name */

  let handle = CreateMutexW(null, INITIAL_OWN, aName);
  let alreadyExists = ctypes.winLastError == ERROR_ALREADY_EXISTS;
  if (handle && !handle.isNull() && !aAllowExisting && alreadyExists) {
    closeHandle(handle);
    handle = null;
  }
  lib.close();

  if (handle && handle.isNull()) {
    handle = null;
  }

  return handle;
}

/**
 * Windows only function that determines a unique mutex name for the
 * installation.
 *
 * @param aGlobal true if the function should return a global mutex. A global
 *                mutex is valid across different sessions
 * @return Global mutex path
 */
function getPerInstallationMutexName(aGlobal = true) {
  if (AppConstants.platform != "win") {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  }

  let hasher = Cc["@mozilla.org/security/hash;1"].
               createInstance(Ci.nsICryptoHash);
  hasher.init(hasher.SHA1);

  let exeFile = Services.dirsvc.get(KEY_EXECUTABLE, Ci.nsILocalFile);

  let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
                  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(exeFile.path.toLowerCase());

  hasher.update(data, data.length);
  return (aGlobal ? "Global\\" : "") + "MozillaUpdateMutex-" + hasher.finish(true);
}

/**
 * Whether or not the current instance has the update mutex. The update mutex
 * gives protection against 2 applications from the same installation updating:
 * 1) Running multiple profiles from the same installation path
 * 2) Two applications running in 2 different user sessions from the same path
 *
 * @return true if this instance holds the update mutex
 */
function hasUpdateMutex() {
  if (AppConstants.platform != "win") {
    return true;
  }
  if (!gUpdateMutexHandle) {
    gUpdateMutexHandle = createMutex(getPerInstallationMutexName(true), false);
  }
  return !!gUpdateMutexHandle;
}

/**
 * Determines whether or not all descendants of a directory are writeable.
 * Note: Does not check the root directory itself for writeability.
 *
 * @return true if all descendants are writeable, false otherwise
 */
function areDirectoryEntriesWriteable(aDir) {
  let items = aDir.directoryEntries;
  while (items.hasMoreElements()) {
    let item = items.getNext().QueryInterface(Ci.nsIFile);
    if (!item.isWritable()) {
      LOG("areDirectoryEntriesWriteable - unable to write to " + item.path);
      return false;
    }
    if (item.isDirectory() && !areDirectoryEntriesWriteable(item)) {
      return false;
    }
  }
  return true;
}

/**
 * OSX only function to determine if the user requires elevation to be able to
 * write to the application bundle.
 *
 * @return true if elevation is required, false otherwise
 */
function getElevationRequired() {
  if (AppConstants.platform != "macosx") {
    return false;
  }

  try {
    // Recursively check that the application bundle (and its descendants) can
    // be written to.
    LOG("getElevationRequired - recursively testing write access on " +
        getInstallDirRoot().path);
    if (!getInstallDirRoot().isWritable() ||
        !areDirectoryEntriesWriteable(getInstallDirRoot())) {
      LOG("getElevationRequired - unable to write to application bundle, " +
          "elevation required");
      return true;
    }
  } catch (ex) {
    LOG("getElevationRequired - unable to write to application bundle, " +
        "elevation required. Exception: " + ex);
    return true;
  }
  LOG("getElevationRequired - able to write to application bundle, elevation " +
      "not required");
  return false;
}

/**
 * Determines whether or not an update can be applied. This is always true on
 * Windows when the service is used. Also, this is always true on OSX because we
 * offer users the option to perform an elevated update when necessary.
 *
 * @return true if an update can be applied, false otherwise
 */
function getCanApplyUpdates() {
  if (AppConstants.platform == "macosx") {
    LOG("getCanApplyUpdates - bypass the write since elevation can be used " +
        "on Mac OS X");
    return true;
  }

  if (shouldUseService()) {
    LOG("getCanApplyUpdates - bypass the write checks because the Windows " +
        "Maintenance Service can be used");
    return true;
  }

  try {
    // Test write access to the updates directory. On Linux the updates
    // directory is located in the installation directory so this is the only
    // write access check that is necessary to tell whether the user can apply
    // updates. On Windows the updates directory is in the user's local
    // application data directory so this should always succeed and additional
    // checks are performed below.
    let updateTestFile = getUpdateFile([FILE_UPDATE_TEST]);
    LOG("getCanApplyUpdates - testing write access " + updateTestFile.path);
    testWriteAccess(updateTestFile, false);
    if (AppConstants.platform == "win") {
      // On Windows when the maintenance service isn't used updates can still be
      // performed in a location requiring admin privileges by the client
      // accepting a UAC prompt from an elevation request made by the updater.
      // Whether the client can elevate (e.g. has a split token) is determined
      // in nsXULAppInfo::GetUserCanElevate which is located in nsAppRunner.cpp.
      let userCanElevate = Services.appinfo.QueryInterface(Ci.nsIWinAppHelper).
                           userCanElevate;
      if (!userCanElevate) {
        // if we're unable to create the test file this will throw an exception.
        let appDirTestFile = getAppBaseDir();
        appDirTestFile.append(FILE_UPDATE_TEST);
        LOG("getCanApplyUpdates - testing write access " + appDirTestFile.path);
        if (appDirTestFile.exists()) {
          appDirTestFile.remove(false);
        }
        appDirTestFile.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
        appDirTestFile.remove(false);
      }
    }
  } catch (e) {
    LOG("getCanApplyUpdates - unable to apply updates. Exception: " + e);
    // No write access to the installation directory
    return false;
  }

  LOG("getCanApplyUpdates - able to apply updates");
  return true;
}

/**
 * Whether or not the application can stage an update for the current session.
 * These checks are only performed once per session due to using a lazy getter.
 *
 * @return true if updates can be staged for this session.
 */
XPCOMUtils.defineLazyGetter(this, "gCanStageUpdatesSession", function aus_gCSUS() {
  if (getElevationRequired()) {
    LOG("gCanStageUpdatesSession - unable to stage updates because elevation " +
        "is required.");
    return false;
  }

  try {
    let updateTestFile;
    if (AppConstants.platform == "macosx") {
      updateTestFile = getUpdateFile([FILE_UPDATE_TEST]);
    } else {
      updateTestFile = getInstallDirRoot();
      updateTestFile.append(FILE_UPDATE_TEST);
    }
    LOG("gCanStageUpdatesSession - testing write access " +
        updateTestFile.path);
    testWriteAccess(updateTestFile, true);
    if (AppConstants.platform != "macosx") {
      // On all platforms except Mac, we need to test the parent directory as
      // well, as we need to be able to move files in that directory during the
      // replacing step.
      updateTestFile = getInstallDirRoot().parent;
      updateTestFile.append(FILE_UPDATE_TEST);
      LOG("gCanStageUpdatesSession - testing write access " +
          updateTestFile.path);
      updateTestFile.createUnique(Ci.nsILocalFile.DIRECTORY_TYPE,
                                  FileUtils.PERMS_DIRECTORY);
      updateTestFile.remove(false);
    }
  } catch (e) {
    LOG("gCanStageUpdatesSession - unable to stage updates. Exception: " +
        e);
    // No write privileges
    return false;
  }

  LOG("gCanStageUpdatesSession - able to stage updates");
  return true;
});

/**
 * Whether or not the application can stage an update.
 *
 * @return true if updates can be staged.
 */
function getCanStageUpdates() {
  // If staging updates are disabled, then just bail out!
  if (!getPref("getBoolPref", PREF_APP_UPDATE_STAGING_ENABLED, false)) {
    LOG("getCanStageUpdates - staging updates is disabled by preference " +
        PREF_APP_UPDATE_STAGING_ENABLED);
    return false;
  }

  if (AppConstants.platform == "win" && shouldUseService()) {
    // No need to perform directory write checks, the maintenance service will
    // be able to write to all directories.
    LOG("getCanStageUpdates - able to stage updates using the service");
    return true;
  }

  if (!hasUpdateMutex()) {
    LOG("getCanStageUpdates - unable to apply updates because another " +
        "instance of the application is already handling updates for this " +
        "installation.");
    return false;
  }

  return gCanStageUpdatesSession;
}

XPCOMUtils.defineLazyGetter(this, "gCanCheckForUpdates", function aus_gCanCheckForUpdates() {
  // If the administrator has disabled app update and locked the preference so
  // users can't check for updates. This preference check is ok in this lazy
  // getter since locked prefs don't change until the application is restarted.
  var enabled = getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true);
  if (!enabled && Services.prefs.prefIsLocked(PREF_APP_UPDATE_ENABLED)) {
    LOG("gCanCheckForUpdates - unable to automatically check for updates, " +
        "the preference is disabled and admistratively locked.");
    return false;
  }

  // If we don't know the binary platform we're updating, we can't update.
  if (!UpdateUtils.ABI) {
    LOG("gCanCheckForUpdates - unable to check for updates, unknown ABI");
    return false;
  }

  // If we don't know the OS version we're updating, we can't update.
  if (!UpdateUtils.OSVersion) {
    LOG("gCanCheckForUpdates - unable to check for updates, unknown OS " +
        "version");
    return false;
  }

  LOG("gCanCheckForUpdates - able to check for updates");
  return true;
});

/**
 * Logs a string to the error console.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  if (gLogEnabled) {
    dump("*** AUS:SVC " + string + "\n");
    Services.console.logStringMessage("AUS:SVC " + string);
  }
}

/**
 * Gets a preference value, handling the case where there is no default.
 * @param   func
 *          The name of the preference function to call, on nsIPrefBranch
 * @param   preference
 *          The name of the preference
 * @param   defaultValue
 *          The default value to return in the event the preference has
 *          no setting
 * @return  The value of the preference, or undefined if there was no
 *          user or default value.
 */
function getPref(func, preference, defaultValue) {
  try {
    return Services.prefs[func](preference);
  } catch (e) {
  }
  return defaultValue;
}

/**
 * Convert a string containing binary values to hex.
 */
function binaryToHex(input) {
  var result = "";
  for (var i = 0; i < input.length; ++i) {
    var hex = input.charCodeAt(i).toString(16);
    if (hex.length == 1)
      hex = "0" + hex;
    result += hex;
  }
  return result;
}

/**
 * Gets the specified directory at the specified hierarchy under the
 * update root directory and creates it if it doesn't exist.
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|
 * @return  nsIFile object for the location specified.
 */
function getUpdateDirCreate(pathArray) {
  return FileUtils.getDir(KEY_UPDROOT, pathArray, true);
}

/**
 * Gets the application base directory.
 *
 * @return  nsIFile object for the application base directory.
 */
function getAppBaseDir() {
  return Services.dirsvc.get(KEY_EXECUTABLE, Ci.nsIFile).parent;
}

/**
 * Gets the root of the installation directory which is the application
 * bundle directory on Mac OS X and the location of the application binary
 * on all other platforms.
 *
 * @return nsIFile object for the directory
 */
function getInstallDirRoot() {
  let dir = getAppBaseDir();
  if (AppConstants.platform == "macosx") {
    // On Mac, we store the Updated.app directory inside the bundle directory.
    dir = dir.parent.parent;
  }
  return dir;
}

/**
 * Gets the file at the specified hierarchy under the update root directory.
 * @param   pathArray
 *          An array of path components to locate beneath the directory
 *          specified by |key|. The last item in this array must be the
 *          leaf name of a file.
 * @return  nsIFile object for the file specified. The file is NOT created
 *          if it does not exist, however all required directories along
 *          the way are.
 */
function getUpdateFile(pathArray) {
  let file = getUpdateDirCreate(pathArray.slice(0, -1));
  file.append(pathArray[pathArray.length - 1]);
  return file;
}

/**
 * Returns human readable status text from the updates.properties bundle
 * based on an error code
 * @param   code
 *          The error code to look up human readable status text for
 * @param   defaultCode
 *          The default code to look up should human readable status text
 *          not exist for |code|
 * @return  A human readable status text string
 */
function getStatusTextFromCode(code, defaultCode) {
  let reason;
  try {
    reason = gUpdateBundle.GetStringFromName("check_error-" + code);
    LOG("getStatusTextFromCode - transfer error: " + reason + ", code: " +
        code);
  } catch (e) {
    // Use the default reason
    reason = gUpdateBundle.GetStringFromName("check_error-" + defaultCode);
    LOG("getStatusTextFromCode - transfer error: " + reason +
        ", default code: " + defaultCode);
  }
  return reason;
}

/**
 * Get the Active Updates directory
 * @return The active updates directory, as a nsIFile object
 */
function getUpdatesDir() {
  // Right now, we only support downloading one patch at a time, so we always
  // use the same target directory.
  return getUpdateDirCreate([DIR_UPDATES, "0"]);
}

/**
 * Reads the update state from the update.status file in the specified
 * directory.
 * @param   dir
 *          The dir to look for an update.status file in
 * @return  The status value of the update.
 */
function readStatusFile(dir) {
  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  let status = readStringFromFile(statusFile) || STATE_NONE;
  LOG("readStatusFile - status: " + status + ", path: " + statusFile.path);
  return status;
}

/**
 * Writes the current update operation/state to a file in the patch
 * directory, indicating to the patching system that operations need
 * to be performed.
 * @param   dir
 *          The patch directory where the update.status file should be
 *          written.
 * @param   state
 *          The state value to write.
 */
function writeStatusFile(dir, state) {
  let statusFile = dir.clone();
  statusFile.append(FILE_UPDATE_STATUS);
  writeStringToFile(statusFile, state);
}

/**
 * Writes the update's application version to a file in the patch directory. If
 * the update doesn't provide application version information via the
 * appVersion attribute the string "null" will be written to the file.
 * This value is compared during startup (in nsUpdateDriver.cpp) to determine if
 * the update should be applied. Note that this won't provide protection from
 * downgrade of the application for the nightly user case where the application
 * version doesn't change.
 * @param   dir
 *          The patch directory where the update.version file should be
 *          written.
 * @param   version
 *          The version value to write. Will be the string "null" when the
 *          update doesn't provide the appVersion attribute in the update xml.
 */
function writeVersionFile(dir, version) {
  let versionFile = dir.clone();
  versionFile.append(FILE_UPDATE_VERSION);
  writeStringToFile(versionFile, version);
}

/**
 * Determines if the service should be used to attempt an update
 * or not.
 *
 * @return  true if the service should be used for updates.
 */
function shouldUseService() {
  // This function will return true if the mantenance service should be used if
  // all of the following conditions are met:
  // 1) This build was done with the maintenance service enabled
  // 2) The maintenance service is installed
  // 3) The pref for using the service is enabled
  if (!AppConstants.MOZ_MAINTENANCE_SERVICE || !isServiceInstalled() ||
      !getPref("getBoolPref", PREF_APP_UPDATE_SERVICE_ENABLED, false)) {
    LOG("shouldUseService - returning false");
    return false;
  }

  LOG("shouldUseService - returning true");
  return true;
}

/**
 * Determines if the service is is installed.
 *
 * @return  true if the service is installed.
 */
function isServiceInstalled() {
  if (!AppConstants.MOZ_MAINTENANCE_SERVICE || AppConstants.platform != "win") {
    LOG("isServiceInstalled - returning false");
    return false;
  }

  let installed = 0;
  try {
    let wrk = Cc["@mozilla.org/windows-registry-key;1"].
              createInstance(Ci.nsIWindowsRegKey);
    wrk.open(wrk.ROOT_KEY_LOCAL_MACHINE,
             "SOFTWARE\\Mozilla\\MaintenanceService",
             wrk.ACCESS_READ | wrk.WOW64_64);
    installed = wrk.readIntValue("Installed");
    wrk.close();
  } catch (e) {
  }
  installed = installed == 1;  // convert to bool
  LOG("isServiceInstalled - returning " + installed);
  return installed;
}

/**
 * Removes the contents of the updates patch directory and rotates the update
 * logs when present. If the update.log exists in the patch directory this will
 * move the last-update.log if it exists to backup-update.log in the parent
 * directory of the patch directory and then move the update.log in the patch
 * directory to last-update.log in the parent directory of the patch directory.
 *
 * @param aRemovePatchFiles (optional, defaults to true)
 *        When true the update's patch directory contents are removed.
 */
function cleanUpUpdatesDir(aRemovePatchFiles = true) {
  let updateDir;
  try {
    updateDir = getUpdatesDir();
  } catch (e) {
    LOG("cleanUpUpdatesDir - unable to get the updates patch directory. " +
        "Exception: " + e);
    return;
  }

  // Preserve the last update log file for debugging purposes.
  let updateLogFile = updateDir.clone();
  updateLogFile.append(FILE_UPDATE_LOG);
  if (updateLogFile.exists()) {
    let dir = updateDir.parent;
    let logFile = dir.clone();
    logFile.append(FILE_LAST_UPDATE_LOG);
    if (logFile.exists()) {
      try {
        logFile.moveTo(dir, FILE_BACKUP_UPDATE_LOG);
      } catch (e) {
        LOG("cleanUpUpdatesDir - failed to rename file " + logFile.path +
            " to " + FILE_BACKUP_UPDATE_LOG);
      }
    }

    try {
      updateLogFile.moveTo(dir, FILE_LAST_UPDATE_LOG);
    } catch (e) {
      LOG("cleanUpUpdatesDir - failed to rename file " + updateLogFile.path +
          " to " + FILE_LAST_UPDATE_LOG);
    }
  }

  if (aRemovePatchFiles) {
    let dirEntries = updateDir.directoryEntries;
    while (dirEntries.hasMoreElements()) {
      let file = dirEntries.getNext().QueryInterface(Ci.nsIFile);
      // Now, recursively remove this file.  The recursive removal is needed for
      // Mac OSX because this directory will contain a copy of updater.app,
      // which is itself a directory and the MozUpdater directory on platforms
      // other than Windows.
      try {
        file.remove(true);
      } catch (e) {
        LOG("cleanUpUpdatesDir - failed to remove file " + file.path);
      }
    }
  }
}

/**
 * Clean up updates list and the updates directory.
 */
function cleanupActiveUpdate() {
  // Move the update from the Active Update list into the Past Updates list.
  var um = Cc["@mozilla.org/updates/update-manager;1"].
           getService(Ci.nsIUpdateManager);
  um.activeUpdate = null;
  um.saveUpdates();

  // Now trash the updates directory, since we're done with it
  cleanUpUpdatesDir();
}

/**
 * An enumeration of items in a JS array.
 * @constructor
 */
function ArrayEnumerator(aItems) {
  this._index = 0;
  if (aItems) {
    for (var i = 0; i < aItems.length; ++i) {
      if (!aItems[i])
        aItems.splice(i, 1);
    }
  }
  this._contents = aItems;
}

ArrayEnumerator.prototype = {
  _index: 0,
  _contents: [],

  hasMoreElements: function ArrayEnumerator_hasMoreElements() {
    return this._index < this._contents.length;
  },

  getNext: function ArrayEnumerator_getNext() {
    return this._contents[this._index++];
  }
};

/**
 * Writes a string of text to a file.  A newline will be appended to the data
 * written to the file.  This function only works with ASCII text.
 */
function writeStringToFile(file, text) {
  let fos = FileUtils.openSafeFileOutputStream(file);
  text += "\n";
  fos.write(text, text.length);
  FileUtils.closeSafeFileOutputStream(fos);
}

function readStringFromInputStream(inputStream) {
  var sis = Cc["@mozilla.org/scriptableinputstream;1"].
            createInstance(Ci.nsIScriptableInputStream);
  sis.init(inputStream);
  var text = sis.read(sis.available());
  sis.close();
  if (text && text[text.length - 1] == "\n") {
    text = text.slice(0, -1);
  }
  return text;
}

/**
 * Reads a string of text from a file.  A trailing newline will be removed
 * before the result is returned.  This function only works with ASCII text.
 */
function readStringFromFile(file) {
  if (!file.exists()) {
    LOG("readStringFromFile - file doesn't exist: " + file.path);
    return null;
  }
  var fis = Cc["@mozilla.org/network/file-input-stream;1"].
            createInstance(Ci.nsIFileInputStream);
  fis.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
  return readStringFromInputStream(fis);
}

function handleUpdateFailure(update, errorCode) {
  update.errorCode = parseInt(errorCode);
  if (WRITE_ERRORS.includes(update.errorCode)) {
    Cc["@mozilla.org/updates/update-prompt;1"].
      createInstance(Ci.nsIUpdatePrompt).
      showUpdateError(update);
    writeStatusFile(getUpdatesDir(), update.state = STATE_PENDING);
    return true;
  }

  if (update.errorCode == ELEVATION_CANCELED) {
    if (getPref("getBoolPref", PREF_APP_UPDATE_DOORHANGER, false)) {
      let elevationAttempts = getPref("getIntPref", PREF_APP_UPDATE_ELEVATE_ATTEMPTS, 0);
      elevationAttempts++;
      Services.prefs.setIntPref(PREF_APP_UPDATE_ELEVATE_ATTEMPTS, elevationAttempts);
      let maxAttempts = Math.min(getPref("getIntPref", PREF_APP_UPDATE_ELEVATE_MAXATTEMPTS, 2), 10);

      if (elevationAttempts > maxAttempts) {
        LOG("handleUpdateFailure - notifying observers of error. " +
            "topic: update-error, status: elevation-attempts-exceeded");
        Services.obs.notifyObservers(update, "update-error", "elevation-attempts-exceeded");
      } else {
        LOG("handleUpdateFailure - notifying observers of error. " +
            "topic: update-error, status: elevation-attempt-failed");
        Services.obs.notifyObservers(update, "update-error", "elevation-attempt-failed");
      }
    }

    let cancelations = getPref("getIntPref", PREF_APP_UPDATE_CANCELATIONS, 0);
    cancelations++;
    Services.prefs.setIntPref(PREF_APP_UPDATE_CANCELATIONS, cancelations);
    if (AppConstants.platform == "macosx") {
      let osxCancelations = getPref("getIntPref",
                                    PREF_APP_UPDATE_CANCELATIONS_OSX, 0);
      osxCancelations++;
      Services.prefs.setIntPref(PREF_APP_UPDATE_CANCELATIONS_OSX,
                                osxCancelations);
      let maxCancels = getPref("getIntPref",
                               PREF_APP_UPDATE_CANCELATIONS_OSX_MAX,
                               DEFAULT_CANCELATIONS_OSX_MAX);
      // Prevent the preference from setting a value greater than 5.
      maxCancels = Math.min(maxCancels, 5);
      if (osxCancelations >= maxCancels) {
        cleanupActiveUpdate();
      } else {
        writeStatusFile(getUpdatesDir(),
                        update.state = STATE_PENDING_ELEVATE);
      }
      update.statusText = gUpdateBundle.GetStringFromName("elevationFailure");
      update.QueryInterface(Ci.nsIWritablePropertyBag);
      update.setProperty("patchingFailed", "elevationFailure");
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateError(update);
    } else {
      writeStatusFile(getUpdatesDir(), update.state = STATE_PENDING);
    }
    return true;
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS);
  }
  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS_OSX)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
  }

  if (SERVICE_ERRORS.includes(update.errorCode)) {
    var failCount = getPref("getIntPref",
                            PREF_APP_UPDATE_SERVICE_ERRORS, 0);
    var maxFail = getPref("getIntPref",
                          PREF_APP_UPDATE_SERVICE_MAXERRORS,
                          DEFAULT_SERVICE_MAX_ERRORS);
    // Prevent the preference from setting a value greater than 10.
    maxFail = Math.min(maxFail, 10);
    // As a safety, when the service reaches maximum failures, it will
    // disable itself and fallback to using the normal update mechanism
    // without the service.
    if (failCount >= maxFail) {
      Services.prefs.setBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, false);
      Services.prefs.clearUserPref(PREF_APP_UPDATE_SERVICE_ERRORS);
    } else {
      failCount++;
      Services.prefs.setIntPref(PREF_APP_UPDATE_SERVICE_ERRORS,
                                failCount);
    }

    writeStatusFile(getUpdatesDir(), update.state = STATE_PENDING);
    return true;
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SERVICE_ERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SERVICE_ERRORS);
  }

  return false;
}

/**
 * Fall back to downloading a complete update in case an update has failed.
 *
 * @param update the update object that has failed to apply.
 * @param postStaging true if we have just attempted to stage an update.
 */
function handleFallbackToCompleteUpdate(update, postStaging) {
  cleanupActiveUpdate();

  update.statusText = gUpdateBundle.GetStringFromName("patchApplyFailure");
  var oldType = update.selectedPatch ? update.selectedPatch.type
                                     : "complete";
  if (update.selectedPatch && oldType == "partial" && update.patchCount == 2) {
    // Partial patch application failed, try downloading the complete
    // update in the background instead.
    LOG("handleFallbackToCompleteUpdate - install of partial patch " +
        "failed, downloading complete patch");
    var status = Cc["@mozilla.org/updates/update-service;1"].
                 getService(Ci.nsIApplicationUpdateService).
                 downloadUpdate(update, !postStaging);
    if (status == STATE_NONE)
      cleanupActiveUpdate();
  } else {
    LOG("handleFallbackToCompleteUpdate - install of complete or " +
        "only one patch offered failed. Notifying observers. topic: " +
        "update-error, status: unknown, " +
        "update.patchCount: " + update.patchCount + ", " +
        "oldType: " + oldType);
    Services.obs.notifyObservers(update, "update-error", "unknown");
  }
  update.QueryInterface(Ci.nsIWritablePropertyBag);
  update.setProperty("patchingFailed", oldType);
}

function pingStateAndStatusCodes(aUpdate, aStartup, aStatus) {
  let patchType = AUSTLMY.PATCH_UNKNOWN;
  if (aUpdate && aUpdate.selectedPatch && aUpdate.selectedPatch.type) {
    if (aUpdate.selectedPatch.type == "complete") {
      patchType = AUSTLMY.PATCH_COMPLETE;
    } else if (aUpdate.selectedPatch.type == "partial") {
      patchType = AUSTLMY.PATCH_PARTIAL;
    }
  }

  let suffix = patchType + "_" + (aStartup ? AUSTLMY.STARTUP : AUSTLMY.STAGE);
  let stateCode = 0;
  let parts = aStatus.split(":");
  if (parts.length > 0) {
    switch (parts[0]) {
      case STATE_NONE:
        stateCode = 2;
        break;
      case STATE_DOWNLOADING:
        stateCode = 3;
        break;
      case STATE_PENDING:
        stateCode = 4;
        break;
      case STATE_PENDING_SERVICE:
        stateCode = 5;
        break;
      case STATE_APPLYING:
        stateCode = 6;
        break;
      case STATE_APPLIED:
        stateCode = 7;
        break;
      case STATE_APPLIED_SERVICE:
        stateCode = 9;
        break;
      case STATE_SUCCEEDED:
        stateCode = 10;
        break;
      case STATE_DOWNLOAD_FAILED:
        stateCode = 11;
        break;
      case STATE_FAILED:
        stateCode = 12;
        break;
      case STATE_PENDING_ELEVATE:
        stateCode = 13;
        break;
      default:
        stateCode = 1;
    }

    if (parts.length > 1) {
      let statusErrorCode = INVALID_UPDATER_STATE_CODE;
      if (parts[0] == STATE_FAILED) {
        statusErrorCode = parseInt(parts[1]) || INVALID_UPDATER_STATUS_CODE;
      }
      AUSTLMY.pingStatusErrorCode(suffix, statusErrorCode);
    }
  }
  AUSTLMY.pingStateCode(suffix, stateCode);
}

/**
 * Update Patch
 * @param   patch
 *          A <patch> element to initialize this object with
 * @throws if patch has a size of 0
 * @constructor
 */
function UpdatePatch(patch) {
  this._properties = {};
  for (var i = 0; i < patch.attributes.length; ++i) {
    var attr = patch.attributes.item(i);
    attr.QueryInterface(Ci.nsIDOMAttr);
    switch (attr.name) {
      case "selected":
        this.selected = attr.value == "true";
        break;
      case "size":
        if (0 == parseInt(attr.value)) {
          LOG("UpdatePatch:init - 0-sized patch!");
          throw Cr.NS_ERROR_ILLEGAL_VALUE;
        }
        // fall through
      default:
        this[attr.name] = attr.value;
        break;
    }
  }
}
UpdatePatch.prototype = {
  /**
   * See nsIUpdateService.idl
   */
  serialize: function UpdatePatch_serialize(updates) {
    var patch = updates.createElementNS(URI_UPDATE_NS, "patch");
    patch.setAttribute("type", this.type);
    patch.setAttribute("URL", this.URL);
    // finalURL is not available until after the download has started
    if (this.finalURL) {
      patch.setAttribute("finalURL", this.finalURL);
    }
    patch.setAttribute("size", this.size);
    if (this.selected) {
      patch.setAttribute("selected", this.selected);
    }
    patch.setAttribute("state", this.state);

    for (let p in this._properties) {
      if (this._properties[p].present) {
        patch.setAttribute(p, this._properties[p].data);
      }
    }

    return patch;
  },

  /**
   * A hash of custom properties
   */
  _properties: null,

  /**
   * See nsIWritablePropertyBag.idl
   */
  setProperty: function UpdatePatch_setProperty(name, value) {
    this._properties[name] = { data: value, present: true };
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  deleteProperty: function UpdatePatch_deleteProperty(name) {
    if (name in this._properties)
      this._properties[name].present = false;
    else
      throw Cr.NS_ERROR_FAILURE;
  },

  /**
   * See nsIPropertyBag.idl
   */
  get enumerator() {
    var properties = [];
    for (var p in this._properties)
      properties.push(this._properties[p].data);
    return new ArrayEnumerator(properties);
  },

  /**
   * See nsIPropertyBag.idl
   * Note: returns null instead of throwing when the property doesn't exist to
   *       simplify code and to silence warnings in debug builds.
   */
  getProperty: function UpdatePatch_getProperty(name) {
    if (name in this._properties &&
        this._properties[name].present) {
      return this._properties[name].data;
    }
    return null;
  },

  /**
   * Returns whether or not the update.status file for this patch exists at the
   * appropriate location.
   */
  get statusFileExists() {
    var statusFile = getUpdatesDir();
    statusFile.append(FILE_UPDATE_STATUS);
    return statusFile.exists();
  },

  /**
   * See nsIUpdateService.idl
   */
  get state() {
    if (this._properties.state)
      return this._properties.state;
    return STATE_NONE;
  },
  set state(val) {
    this._properties.state = val;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePatch,
                                         Ci.nsIPropertyBag,
                                         Ci.nsIWritablePropertyBag])
};

/**
 * Update
 * Implements nsIUpdate
 * @param   update
 *          An <update> element to initialize this object with
 * @throws if the update contains no patches
 * @constructor
 */
function Update(update) {
  this._properties = {};
  this._patches = [];
  this.isCompleteUpdate = false;
  this.unsupported = false;
  this.channel = "default";
  this.promptWaitTime = getPref("getIntPref", PREF_APP_UPDATE_PROMPTWAITTIME, 43200);
  this.backgroundInterval = getPref("getIntPref", PREF_APP_UPDATE_BACKGROUNDINTERVAL,
                                    DOWNLOAD_BACKGROUND_INTERVAL);

  // Null <update>, assume this is a message container and do no
  // further initialization
  if (!update) {
    return;
  }

  const ELEMENT_NODE = Ci.nsIDOMNode.ELEMENT_NODE;
  let patch;
  for (let i = 0; i < update.childNodes.length; ++i) {
    let patchElement = update.childNodes.item(i);
    if (patchElement.nodeType != ELEMENT_NODE ||
        patchElement.localName != "patch") {
      continue;
    }

    patchElement.QueryInterface(Ci.nsIDOMElement);
    try {
      patch = new UpdatePatch(patchElement);
    } catch (e) {
      continue;
    }
    this._patches.push(patch);
  }

  if (this._patches.length == 0 && !update.hasAttribute("unsupported")) {
    throw Cr.NS_ERROR_ILLEGAL_VALUE;
  }

  // Set the installDate value with the current time. If the update has an
  // installDate attribute this will be replaced with that value if it doesn't
  // equal 0.
  this.installDate = (new Date()).getTime();

  for (let i = 0; i < update.attributes.length; ++i) {
    var attr = update.attributes.item(i);
    attr.QueryInterface(Ci.nsIDOMAttr);
    if (attr.value == "undefined") {
      continue;
    } else if (attr.name == "detailsURL") {
      this._detailsURL = attr.value;
    } else if (attr.name == "installDate" && attr.value) {
      let val = parseInt(attr.value);
      if (val) {
        this.installDate = val;
      }
    } else if (attr.name == "isCompleteUpdate") {
      this.isCompleteUpdate = attr.value == "true";
    } else if (attr.name == "promptWaitTime") {
      if (!isNaN(attr.value)) {
        this.promptWaitTime = parseInt(attr.value);
      }
    } else if (attr.name == "backgroundInterval") {
      if (!isNaN(attr.value)) {
        this.backgroundInterval = parseInt(attr.value);
      }
    } else if (attr.name == "unsupported") {
      this.unsupported = attr.value == "true";
    } else {
      this[attr.name] = attr.value;

      switch (attr.name) {
        case "appVersion":
        case "buildID":
        case "channel":
        case "displayVersion":
        case "name":
        case "previousAppVersion":
        case "serviceURL":
        case "statusText":
        case "type":
          break;
        default:
          // Save custom attributes when serializing to the local xml file but
          // don't use this method for the expected attributes which are already
          // handled in serialize.
          this.setProperty(attr.name, attr.value);
          break;
      }
    }
  }

  if (!this.displayVersion) {
    this.displayVersion = this.appVersion;
  }

  // Don't allow the background download interval to be greater than 10 minutes.
  this.backgroundInterval = Math.min(this.backgroundInterval, 600);

  // The Update Name is either the string provided by the <update> element, or
  // the string: "<App Name> <Update App Version>"
  var name = "";
  if (update.hasAttribute("name")) {
    name = update.getAttribute("name");
  } else {
    var brandBundle = Services.strings.createBundle(URI_BRAND_PROPERTIES);
    var appName = brandBundle.GetStringFromName("brandShortName");
    name = gUpdateBundle.formatStringFromName("updateName",
                                              [appName, this.displayVersion], 2);
  }
  this.name = name;
}
Update.prototype = {
  /**
   * See nsIUpdateService.idl
   */
  get patchCount() {
    return this._patches.length;
  },

  /**
   * See nsIUpdateService.idl
   */
  getPatchAt: function Update_getPatchAt(index) {
    return this._patches[index];
  },

  /**
   * See nsIUpdateService.idl
   *
   * We use a copy of the state cached on this object in |_state| only when
   * there is no selected patch, i.e. in the case when we could not load
   * |.activeUpdate| from the update manager for some reason but still have
   * the update.status file to work with.
   */
  _state: "",
  set state(state) {
    if (this.selectedPatch)
      this.selectedPatch.state = state;
    this._state = state;
    return state;
  },
  get state() {
    if (this.selectedPatch)
      return this.selectedPatch.state;
    return this._state;
  },

  /**
   * See nsIUpdateService.idl
   */
  errorCode: 0,

  /**
   * See nsIUpdateService.idl
   */
  get selectedPatch() {
    for (var i = 0; i < this.patchCount; ++i) {
      if (this._patches[i].selected)
        return this._patches[i];
    }
    return null;
  },

  /**
   * See nsIUpdateService.idl
   */
  get detailsURL() {
    if (!this._detailsURL) {
      try {
        // Try using a default details URL supplied by the distribution
        // if the update XML does not supply one.
        return Services.urlFormatter.formatURLPref(PREF_APP_UPDATE_URL_DETAILS);
      } catch (e) {
      }
    }
    return this._detailsURL || "";
  },

  /**
   * See nsIUpdateService.idl
   */
  serialize: function Update_serialize(updates) {
    // If appVersion isn't defined just return null. This happens when cleaning
    // up invalid updates (e.g. incorrect channel).
    if (!this.appVersion) {
      return null;
    }
    var update = updates.createElementNS(URI_UPDATE_NS, "update");
    update.setAttribute("appVersion", this.appVersion);
    update.setAttribute("buildID", this.buildID);
    update.setAttribute("channel", this.channel);
    update.setAttribute("displayVersion", this.displayVersion);
    update.setAttribute("installDate", this.installDate);
    update.setAttribute("isCompleteUpdate", this.isCompleteUpdate);
    update.setAttribute("name", this.name);
    update.setAttribute("serviceURL", this.serviceURL);
    update.setAttribute("promptWaitTime", this.promptWaitTime);
    update.setAttribute("backgroundInterval", this.backgroundInterval);
    update.setAttribute("type", this.type);

    if (this.detailsURL) {
      update.setAttribute("detailsURL", this.detailsURL);
    }
    if (this.previousAppVersion) {
      update.setAttribute("previousAppVersion", this.previousAppVersion);
    }
    if (this.statusText) {
      update.setAttribute("statusText", this.statusText);
    }
    if (this.unsupported) {
      update.setAttribute("unsupported", this.unsupported);
    }
    updates.documentElement.appendChild(update);

    for (let p in this._properties) {
      if (this._properties[p].present) {
        update.setAttribute(p, this._properties[p].data);
      }
    }

    for (let i = 0; i < this.patchCount; ++i) {
      update.appendChild(this.getPatchAt(i).serialize(updates));
    }

    return update;
  },

  /**
   * A hash of custom properties
   */
  _properties: null,

  /**
   * See nsIWritablePropertyBag.idl
   */
  setProperty: function Update_setProperty(name, value) {
    this._properties[name] = { data: value, present: true };
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  deleteProperty: function Update_deleteProperty(name) {
    if (name in this._properties)
      this._properties[name].present = false;
    else
      throw Cr.NS_ERROR_FAILURE;
  },

  /**
   * See nsIPropertyBag.idl
   */
  get enumerator() {
    var properties = [];
    for (let p in this._properties) {
      properties.push(this._properties[p].data);
    }
    return new ArrayEnumerator(properties);
  },

  /**
   * See nsIPropertyBag.idl
   * Note: returns null instead of throwing when the property doesn't exist to
   *       simplify code and to silence warnings in debug builds.
   */
  getProperty: function Update_getProperty(name) {
    if (name in this._properties && this._properties[name].present) {
      return this._properties[name].data;
    }
    return null;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdate,
                                         Ci.nsIPropertyBag,
                                         Ci.nsIWritablePropertyBag])
};

const UpdateServiceFactory = {
  _instance: null,
  createInstance(outer, iid) {
    if (outer != null)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this._instance == null ? this._instance = new UpdateService() :
                                    this._instance;
  }
};

/**
 * UpdateService
 * A Service for managing the discovery and installation of software updates.
 * @constructor
 */
function UpdateService() {
  LOG("Creating UpdateService");
  Services.obs.addObserver(this, "xpcom-shutdown");
  Services.prefs.addObserver(PREF_APP_UPDATE_LOG, this);
}

UpdateService.prototype = {
  /**
   * The downloader we are using to download updates. There is only ever one of
   * these.
   */
  _downloader: null,

  /**
   * Whether or not the service registered the "online" observer.
   */
  _registeredOnlineObserver: false,

  /**
   * The current number of consecutive socket errors
   */
  _consecutiveSocketErrors: 0,

  /**
   * A timer used to retry socket errors
   */
  _retryTimer: null,

  /**
   * Whether or not a background update check was initiated by the
   * application update timer notification.
   */
  _isNotify: true,

  /**
   * Handle Observer Service notifications
   * @param   subject
   *          The subject of the notification
   * @param   topic
   *          The notification name
   * @param   data
   *          Additional data
   */
  observe: function AUS_observe(subject, topic, data) {
    switch (topic) {
      case "post-update-processing":
        if (readStatusFile(getUpdatesDir()) == STATE_SUCCEEDED) {
          // After a successful update the post update preference needs to be
          // set early during startup so applications can perform post update
          // actions when they are defined in the update's metadata.
          Services.prefs.setBoolPref(PREF_APP_UPDATE_POSTUPDATE, true);
        }

        if (Services.appinfo.ID in APPID_TO_TOPIC) {
          // Delay post-update processing to ensure that possible update
          // dialogs are shown in front of the app window, if possible.
          // See bug 311614.
          Services.obs.addObserver(this, APPID_TO_TOPIC[Services.appinfo.ID]);
          break;
        }
        // intentional fallthrough
      case "sessionstore-windows-restored":
      case "mail-startup-done":
      case "xul-window-visible":
        if (Services.appinfo.ID in APPID_TO_TOPIC) {
          Services.obs.removeObserver(this,
                                      APPID_TO_TOPIC[Services.appinfo.ID]);
        }
        // intentional fallthrough
      case "test-post-update-processing":
        // Clean up any extant updates
        this._postUpdateProcessing();
        break;
      case "network:offline-status-changed":
        this._offlineStatusChanged(data);
        break;
      case "nsPref:changed":
        if (data == PREF_APP_UPDATE_LOG) {
          gLogEnabled = getPref("getBoolPref", PREF_APP_UPDATE_LOG, false);
        }
        break;
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, topic);
        Services.prefs.removeObserver(PREF_APP_UPDATE_LOG, this);

        if (AppConstants.platform == "win" && gUpdateMutexHandle) {
          // If we hold the update mutex, let it go!
          // The OS would clean this up sometime after shutdown,
          // but that would have no guarantee on timing.
          closeHandle(gUpdateMutexHandle);
        }
        if (this._retryTimer) {
          this._retryTimer.cancel();
        }

        this.pauseDownload();
        // Prevent leaking the downloader (bug 454964)
        this._downloader = null;
        break;
    }
  },

  /**
   * The following needs to happen during the post-update-processing
   * notification from nsUpdateServiceStub.js:
   * 1. post update processing
   * 2. resume of a download that was in progress during a previous session
   * 3. start of a complete update download after the failure to apply a partial
   *    update
   */

  /**
   * Perform post-processing on updates lingering in the updates directory
   * from a previous application session - either report install failures (and
   * optionally attempt to fetch a different version if appropriate) or
   * notify the user of install success.
   */
  _postUpdateProcessing: function AUS__postUpdateProcessing() {
    if (!this.canCheckForUpdates) {
      LOG("UpdateService:_postUpdateProcessing - unable to check for " +
          "updates... returning early");
      return;
    }

    if (!this.canApplyUpdates) {
      LOG("UpdateService:_postUpdateProcessing - unable to apply " +
          "updates... returning early");
      // If the update is present in the update directly somehow,
      // it would prevent us from notifying the user of futher updates.
      cleanupActiveUpdate();
      return;
    }

    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    var update = um.activeUpdate;
    var status = readStatusFile(getUpdatesDir());
    pingStateAndStatusCodes(update, true, status);
    // STATE_NONE status typically means that the update.status file is present
    // but a background download error occurred.
    if (status == STATE_NONE) {
      LOG("UpdateService:_postUpdateProcessing - no status, no update");
      cleanupActiveUpdate();
      return;
    }

    // Handle the case when the update is the same or older than the current
    // version and nsUpdateDriver.cpp skipped updating due to the version being
    // older than the current version.
    if (update && update.appVersion &&
        (status == STATE_PENDING || status == STATE_PENDING_SERVICE ||
         status == STATE_APPLIED || status == STATE_APPLIED_SERVICE ||
         status == STATE_PENDING_ELEVATE)) {
      if (Services.vc.compare(update.appVersion, Services.appinfo.version) < 0 ||
          Services.vc.compare(update.appVersion, Services.appinfo.version) == 0 &&
          update.buildID == Services.appinfo.appBuildID) {
        LOG("UpdateService:_postUpdateProcessing - removing update for older " +
            "or same application version");
        cleanupActiveUpdate();
        return;
      }
    }

    if (status == STATE_DOWNLOADING) {
      LOG("UpdateService:_postUpdateProcessing - patch found in downloading " +
          "state");
      if (update && update.state != STATE_SUCCEEDED) {
        // Resume download
        status = this.downloadUpdate(update, true);
        if (status == STATE_NONE)
          cleanupActiveUpdate();
      }
      return;
    }

    if (status == STATE_APPLYING) {
      // This indicates that the background updater service is in either of the
      // following two states:
      // 1. It is in the process of applying an update in the background, and
      //    we just happen to be racing against that.
      // 2. It has failed to apply an update for some reason, and we hit this
      //    case because the updater process has set the update status to
      //    applying, but has never finished.
      // In order to differentiate between these two states, we look at the
      // state field of the update object.  If it's "pending", then we know
      // that this is the first time we're hitting this case, so we switch
      // that state to "applying" and we just wait and hope for the best.
      // If it's "applying", we know that we've already been here once, so
      // we really want to start from a clean state.
      if (update &&
          (update.state == STATE_PENDING ||
           update.state == STATE_PENDING_SERVICE)) {
        LOG("UpdateService:_postUpdateProcessing - patch found in applying " +
            "state for the first time");
        update.state = STATE_APPLYING;
        um.saveUpdates();
      } else { // We get here even if we don't have an update object
        LOG("UpdateService:_postUpdateProcessing - patch found in applying " +
            "state for the second time");
        cleanupActiveUpdate();
      }
      return;
    }

    if (!update) {
      if (status != STATE_SUCCEEDED) {
        LOG("UpdateService:_postUpdateProcessing - previous patch failed " +
            "and no patch available");
        cleanupActiveUpdate();
        return;
      }
      update = new Update(null);
    }

    let parts = status.split(":");
    update.state = parts[0];
    if (update.state == STATE_FAILED && parts[1]) {
      update.errorCode = parseInt(parts[1]);
    }


    if (status != STATE_SUCCEEDED) {
      // Since the update didn't succeed save a copy of the active update's
      // current state to the updates.xml so it is possible to track failures.
      um.saveUpdates();
      // Rotate the update logs so the update log isn't removed. By passing
      // false the patch directory won't be removed.
      cleanUpUpdatesDir(false);
    }

    if (status == STATE_SUCCEEDED) {
      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS)) {
        Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS);
      }
      update.statusText = gUpdateBundle.GetStringFromName("installSuccess");

      // Update the patch's metadata.
      um.activeUpdate = update;

      // Done with this update. Clean it up.
      cleanupActiveUpdate();

      Services.prefs.setIntPref(PREF_APP_UPDATE_ELEVATE_ATTEMPTS, 0);
    } else if (status == STATE_PENDING_ELEVATE) {
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateElevationRequired();
    } else {
      // If there was an I/O error it is assumed that the patch is not invalid
      // and it is set to pending so an attempt to apply it again will happen
      // when the application is restarted.
      if (update.state == STATE_FAILED && update.errorCode) {
        if (handleUpdateFailure(update, update.errorCode)) {
          return;
        }
      }

      // Something went wrong with the patch application process.
      handleFallbackToCompleteUpdate(update, false);
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateError(update);
    }
  },

  /**
   * Register an observer when the network comes online, so we can short-circuit
   * the app.update.interval when there isn't connectivity
   */
  _registerOnlineObserver: function AUS__registerOnlineObserver() {
    if (this._registeredOnlineObserver) {
      LOG("UpdateService:_registerOnlineObserver - observer already registered");
      return;
    }

    LOG("UpdateService:_registerOnlineObserver - waiting for the network to " +
        "be online, then forcing another check");

    Services.obs.addObserver(this, "network:offline-status-changed");
    this._registeredOnlineObserver = true;
  },

  /**
   * Called from the network:offline-status-changed observer.
   */
  _offlineStatusChanged: function AUS__offlineStatusChanged(status) {
    if (status !== "online") {
      return;
    }

    Services.obs.removeObserver(this, "network:offline-status-changed");
    this._registeredOnlineObserver = false;

    LOG("UpdateService:_offlineStatusChanged - network is online, forcing " +
        "another background check");

    // the background checker is contained in notify
    this._attemptResume();
  },

  onCheckComplete: function AUS_onCheckComplete(request, updates, updateCount) {
    this._selectAndInstallUpdate(updates);
  },

  onError: function AUS_onError(request, update) {
    LOG("UpdateService:onError - error during background update. error code: " +
        update.errorCode + ", status text: " + update.statusText);

    if (update.errorCode == NETWORK_ERROR_OFFLINE) {
      // Register an online observer to try again
      this._registerOnlineObserver();
      if (this._pingSuffix) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_OFFLINE);
      }
      return;
    }

    // Send the error code to telemetry
    AUSTLMY.pingCheckExError(this._pingSuffix, update.errorCode);
    update.errorCode = BACKGROUNDCHECK_MULTIPLE_FAILURES;
    let errCount = getPref("getIntPref", PREF_APP_UPDATE_BACKGROUNDERRORS, 0);
    errCount++;
    Services.prefs.setIntPref(PREF_APP_UPDATE_BACKGROUNDERRORS, errCount);
    // Don't allow the preference to set a value greater than 20 for max errors.
    let maxErrors = Math.min(getPref("getIntPref", PREF_APP_UPDATE_BACKGROUNDMAXERRORS, 10), 20);

    if (errCount >= maxErrors) {
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      LOG("UpdateService:onError - notifying observers of error. " +
          "topic: update-error, status: check-attempts-exceeded");
      Services.obs.notifyObservers(update, "update-error", "check-attempts-exceeded");
      prompter.showUpdateError(update);
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_GENERAL_ERROR_PROMPT);
    } else {
      LOG("UpdateService:onError - notifying observers of error. " +
          "topic: update-error, status: check-attempt-failed");
      Services.obs.notifyObservers(update, "update-error", "check-attempt-failed");
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_GENERAL_ERROR_SILENT);
    }
  },

  /**
   * Called when a connection should be resumed
   */
  _attemptResume: function AUS_attemptResume() {
    LOG("UpdateService:_attemptResume");
    // If a download is in progress, then resume it.
    if (this._downloader && this._downloader._patch &&
        this._downloader._patch.state == STATE_DOWNLOADING &&
        this._downloader._update) {
      LOG("UpdateService:_attemptResume - _patch.state: " +
          this._downloader._patch.state);
      // Make sure downloading is the state for selectPatch to work correctly
      writeStatusFile(getUpdatesDir(), STATE_DOWNLOADING);
      var status = this.downloadUpdate(this._downloader._update,
                                       this._downloader.background);
      LOG("UpdateService:_attemptResume - downloadUpdate status: " + status);
      if (status == STATE_NONE) {
        cleanupActiveUpdate();
      }
      return;
    }

    this.backgroundChecker.checkForUpdates(this, false);
  },

  /**
   * Notified when a timer fires
   * @param   timer
   *          The timer that fired
   */
  notify: function AUS_notify(timer) {
    this._checkForBackgroundUpdates(true);
  },

  /**
   * See nsIUpdateService.idl
   */
  checkForBackgroundUpdates: function AUS_checkForBackgroundUpdates() {
    this._checkForBackgroundUpdates(false);
  },

  // The suffix used for background update check telemetry histogram ID's.
  get _pingSuffix() {
    return this._isNotify ? AUSTLMY.NOTIFY : AUSTLMY.EXTERNAL;
  },

  /**
   * Checks for updates in the background.
   * @param   isNotify
   *          Whether or not a background update check was initiated by the
   *          application update timer notification.
   */
  _checkForBackgroundUpdates: function AUS__checkForBackgroundUpdates(isNotify) {
    this._isNotify = isNotify;

    // Histogram IDs:
    // UPDATE_PING_COUNT_EXTERNAL
    // UPDATE_PING_COUNT_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_PING_COUNT_" + this._pingSuffix,
                        true, false);

    // Histogram IDs:
    // UPDATE_UNABLE_TO_APPLY_EXTERNAL
    // UPDATE_UNABLE_TO_APPLY_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_UNABLE_TO_APPLY_" + this._pingSuffix,
                        getCanApplyUpdates(), true);
    // Histogram IDs:
    // UPDATE_CANNOT_STAGE_EXTERNAL
    // UPDATE_CANNOT_STAGE_NOTIFY
    AUSTLMY.pingGeneric("UPDATE_CANNOT_STAGE_" + this._pingSuffix,
                        getCanStageUpdates(), true);
    // Histogram IDs:
    // UPDATE_INVALID_LASTUPDATETIME_EXTERNAL
    // UPDATE_INVALID_LASTUPDATETIME_NOTIFY
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_EXTERNAL
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_NOTIFY
    AUSTLMY.pingLastUpdateTime(this._pingSuffix);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_ENABLED_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_ENABLED_NOTIFY
    AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_ENABLED_" + this._pingSuffix,
                         PREF_APP_UPDATE_ENABLED, true, true);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_AUTO_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_AUTO_NOTIFY
    AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_AUTO_" + this._pingSuffix,
                         PREF_APP_UPDATE_AUTO, true, true);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_NOTIFY
    AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_" +
                         this._pingSuffix,
                         PREF_APP_UPDATE_STAGING_ENABLED, true, true);
    if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
      // Histogram IDs:
      // UPDATE_PREF_UPDATE_CANCELATIONS_EXTERNAL
      // UPDATE_PREF_UPDATE_CANCELATIONS_NOTIFY
      AUSTLMY.pingIntPref("UPDATE_PREF_UPDATE_CANCELATIONS_" + this._pingSuffix,
                          PREF_APP_UPDATE_CANCELATIONS, 0, 0);
    }
    if (AppConstants.platform == "macosx") {
      // Histogram IDs:
      // UPDATE_PREF_UPDATE_CANCELATIONS_OSX_EXTERNAL
      // UPDATE_PREF_UPDATE_CANCELATIONS_OSX_NOTIFY
      AUSTLMY.pingIntPref("UPDATE_PREF_UPDATE_CANCELATIONS_OSX_" +
                          this._pingSuffix,
                          PREF_APP_UPDATE_CANCELATIONS_OSX, 0, 0);
    }
    if (AppConstants.MOZ_MAINTENANCE_SERVICE) {
      // Histogram IDs:
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_EXTERNAL
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_NOTIFY
      AUSTLMY.pingBoolPref("UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_" +
                           this._pingSuffix,
                           PREF_APP_UPDATE_SERVICE_ENABLED, true);
      // Histogram IDs:
      // UPDATE_PREF_SERVICE_ERRORS_EXTERNAL
      // UPDATE_PREF_SERVICE_ERRORS_NOTIFY
      AUSTLMY.pingIntPref("UPDATE_PREF_SERVICE_ERRORS_" + this._pingSuffix,
                          PREF_APP_UPDATE_SERVICE_ERRORS, 0, 0);
      if (AppConstants.platform == "win") {
        // Histogram IDs:
        // UPDATE_SERVICE_INSTALLED_EXTERNAL
        // UPDATE_SERVICE_INSTALLED_NOTIFY
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_EXTERNAL
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_NOTIFY
        AUSTLMY.pingServiceInstallStatus(this._pingSuffix, isServiceInstalled());
      }
    }

    // If a download is in progress or the patch has been staged do nothing.
    if (this.isDownloading) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADING);
      return;
    }

    if (this._downloader && this._downloader.patchIsStaged) {
      let readState = readStatusFile(getUpdatesDir());
      if (readState == STATE_PENDING || readState == STATE_PENDING_SERVICE ||
          readState == STATE_PENDING_ELEVATE) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADED);
      } else {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_STAGED);
      }
      return;
    }

    let validUpdateURL = true;
    this.backgroundChecker.getUpdateURL(false).catch(e => {
      validUpdateURL = false;
    }).then(() => {
      // The following checks are done here so they can be differentiated from
      // foreground checks.
      if (!UpdateUtils.OSVersion) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_OS_VERSION);
      } else if (!UpdateUtils.ABI) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_OS_ABI);
      } else if (!validUpdateURL) {
        AUSTLMY.pingCheckCode(this._pingSuffix,
                              AUSTLMY.CHK_INVALID_DEFAULT_URL);
      } else if (!getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true)) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_PREF_DISABLED);
      } else if (!hasUpdateMutex()) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_MUTEX);
      } else if (!gCanCheckForUpdates) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNABLE_TO_CHECK);
      } else if (!this.backgroundChecker._enabled) {
        AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_DISABLED_FOR_SESSION);
      }

      this.backgroundChecker.checkForUpdates(this, false);
    });
  },

  /**
   * Determine the update from the specified updates that should be offered.
   * If both valid major and minor updates are available the minor update will
   * be offered.
   * @param   updates
   *          An array of available nsIUpdate items
   * @return  The nsIUpdate to offer.
   */
  selectUpdate: function AUS_selectUpdate(updates) {
    if (updates.length == 0) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_UPDATE_FOUND);
      return null;
    }

    // The ping for unsupported is sent after the call to showPrompt.
    if (updates.length == 1 && updates[0].unsupported) {
      return updates[0];
    }

    // Choose the newest of the available minor and major updates.
    var majorUpdate = null;
    var minorUpdate = null;
    var vc = Services.vc;
    let lastCheckCode = AUSTLMY.CHK_NO_COMPAT_UPDATE_FOUND;

    updates.forEach(function(aUpdate) {
      // Ignore updates for older versions of the application and updates for
      // the same version of the application with the same build ID.
      if (vc.compare(aUpdate.appVersion, Services.appinfo.version) < 0 ||
          vc.compare(aUpdate.appVersion, Services.appinfo.version) == 0 &&
          aUpdate.buildID == Services.appinfo.appBuildID) {
        LOG("UpdateService:selectUpdate - skipping update because the " +
            "update's application version is less than the current " +
            "application version");
        lastCheckCode = AUSTLMY.CHK_UPDATE_PREVIOUS_VERSION;
        return;
      }

      switch (aUpdate.type) {
        case "major":
          if (!majorUpdate)
            majorUpdate = aUpdate;
          else if (vc.compare(majorUpdate.appVersion, aUpdate.appVersion) <= 0)
            majorUpdate = aUpdate;
          break;
        case "minor":
          if (!minorUpdate)
            minorUpdate = aUpdate;
          else if (vc.compare(minorUpdate.appVersion, aUpdate.appVersion) <= 0)
            minorUpdate = aUpdate;
          break;
        default:
          LOG("UpdateService:selectUpdate - skipping unknown update type: " +
              aUpdate.type);
          lastCheckCode = AUSTLMY.CHK_UPDATE_INVALID_TYPE;
          break;
      }
    });

    let update = minorUpdate || majorUpdate;
    if (AppConstants.platform == "macosx" && update) {
      if (getElevationRequired()) {
        let installAttemptVersion = getPref("getCharPref",
                                            PREF_APP_UPDATE_ELEVATE_VERSION,
                                            null);
        if (vc.compare(installAttemptVersion, update.appVersion) != 0) {
          Services.prefs.setCharPref(PREF_APP_UPDATE_ELEVATE_VERSION,
                                     update.appVersion);
          if (Services.prefs.prefHasUserValue(
                PREF_APP_UPDATE_CANCELATIONS_OSX)) {
            Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
          }
          if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_NEVER)) {
            Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_NEVER);
          }
        } else {
          let numCancels = getPref("getIntPref",
                                   PREF_APP_UPDATE_CANCELATIONS_OSX, 0);
          let rejectedVersion = getPref("getCharPref",
                                        PREF_APP_UPDATE_ELEVATE_NEVER, "");
          let maxCancels = getPref("getIntPref",
                                   PREF_APP_UPDATE_CANCELATIONS_OSX_MAX,
                                   DEFAULT_CANCELATIONS_OSX_MAX);
          if (numCancels >= maxCancels) {
            LOG("UpdateService:selectUpdate - the user requires elevation to " +
                "install this update, but the user has exceeded the max " +
                "number of elevation attempts.");
            update.elevationFailure = true;
            AUSTLMY.pingCheckCode(
              this._pingSuffix,
              AUSTLMY.CHK_ELEVATION_DISABLED_FOR_VERSION);
          } else if (vc.compare(rejectedVersion, update.appVersion) == 0) {
            LOG("UpdateService:selectUpdate - the user requires elevation to " +
                "install this update, but elevation is disabled for this " +
                "version.");
            update.elevationFailure = true;
            AUSTLMY.pingCheckCode(this._pingSuffix,
                                  AUSTLMY.CHK_ELEVATION_OPTOUT_FOR_VERSION);
          } else {
            LOG("UpdateService:selectUpdate - the user requires elevation to " +
                "install the update.");
          }
        }
      } else {
        // Clear elevation-related prefs since they no longer apply (the user
        // may have gained write access to the Firefox directory or an update
        // was executed with a different profile).
        if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_VERSION)) {
          Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_VERSION);
        }
        if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS_OSX)) {
          Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
        }
        if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_NEVER)) {
          Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_NEVER);
        }
      }
    } else if (!update) {
      AUSTLMY.pingCheckCode(this._pingSuffix, lastCheckCode);
    }

    return update;
  },

  /**
   * Determine which of the specified updates should be installed and begin the
   * download/installation process or notify the user about the update.
   * @param   updates
   *          An array of available updates
   */
  _selectAndInstallUpdate: function AUS__selectAndInstallUpdate(updates) {
    // Return early if there's an active update. The user is already aware and
    // is downloading or performed some user action to prevent notification.
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    if (um.activeUpdate) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_HAS_ACTIVEUPDATE);
      return;
    }

    var updateEnabled = getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true);
    if (!updateEnabled) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_PREF_DISABLED);
      LOG("UpdateService:_selectAndInstallUpdate - not prompting because " +
          "update is disabled");
      return;
    }

    var update = this.selectUpdate(updates, updates.length);
    if (!update || update.elevationFailure) {
      return;
    }

    if (update.unsupported) {
      LOG("UpdateService:_selectAndInstallUpdate - update not supported for " +
          "this system. Notifying observers. topic: update-available, " +
          "status: unsupported");
      if (!getPref("getBoolPref", PREF_APP_UPDATE_NOTIFIEDUNSUPPORTED, false)) {
        LOG("UpdateService:_selectAndInstallUpdate - notifying that the " +
            "update is not supported for this system");
        this._showPrompt(update);
      }

      Services.obs.notifyObservers(null, "update-available", "unsupported");
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNSUPPORTED);
      return;
    }

    if (!getCanApplyUpdates()) {
      LOG("UpdateService:_selectAndInstallUpdate - the user is unable to " +
          "apply updates... prompting. Notifying observers. " +
          "topic: update-available, status: cant-apply");

      Services.obs.notifyObservers(null, "update-available", "cant-apply");
      this._showPrompt(update);
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNABLE_TO_APPLY);
      return;
    }

    /**
     * From this point on there are two possible outcomes:
     * 1. download and install the update automatically
     * 2. notify the user about the availability of an update
     *
     * Notes:
     * a) if the app.update.auto preference is false then automatic download and
     *    install is disabled and the user will be notified.
     *
     * If the update when it is first read does not have an appVersion attribute
     * the following deprecated behavior will occur:
     * Update Type   Outcome
     * Major         Notify
     * Minor         Auto Install
     */
    if (!getPref("getBoolPref", PREF_APP_UPDATE_AUTO, true)) {
      LOG("UpdateService:_selectAndInstallUpdate - prompting because silent " +
          "install is disabled. Notifying observers. topic: update-available, " +
          "status: show-prompt");
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_SHOWPROMPT_PREF);

      Services.obs.notifyObservers(update, "update-available", "show-prompt");
      this._showPrompt(update);
      return;
    }

    LOG("UpdateService:_selectAndInstallUpdate - download the update");
    let status = this.downloadUpdate(update, true);
    if (status == STATE_NONE) {
      cleanupActiveUpdate();
    }
    AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_DOWNLOAD_UPDATE);
  },

  _showPrompt: function AUS__showPrompt(update) {
    let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                   createInstance(Ci.nsIUpdatePrompt);
    prompter.showUpdateAvailable(update);
  },

  /**
   * The Checker used for background update checks.
   */
  _backgroundChecker: null,

  /**
   * See nsIUpdateService.idl
   */
  get backgroundChecker() {
    if (!this._backgroundChecker)
      this._backgroundChecker = new Checker();
    return this._backgroundChecker;
  },

  /**
   * See nsIUpdateService.idl
   */
  get canCheckForUpdates() {
    return gCanCheckForUpdates && hasUpdateMutex();
  },

  /**
   * See nsIUpdateService.idl
   */
  get elevationRequired() {
    return getElevationRequired();
  },

  /**
   * See nsIUpdateService.idl
   */
  get canApplyUpdates() {
    return getCanApplyUpdates() && hasUpdateMutex();
  },

  /**
   * See nsIUpdateService.idl
   */
  get canStageUpdates() {
    return getCanStageUpdates();
  },

  /**
   * See nsIUpdateService.idl
   */
  get isOtherInstanceHandlingUpdates() {
    return !hasUpdateMutex();
  },


  /**
   * See nsIUpdateService.idl
   */
  addDownloadListener: function AUS_addDownloadListener(listener) {
    if (!this._downloader) {
      LOG("UpdateService:addDownloadListener - no downloader!");
      return;
    }
    this._downloader.addDownloadListener(listener);
  },

  /**
   * See nsIUpdateService.idl
   */
  removeDownloadListener: function AUS_removeDownloadListener(listener) {
    if (!this._downloader) {
      LOG("UpdateService:removeDownloadListener - no downloader!");
      return;
    }
    this._downloader.removeDownloadListener(listener);
  },

  /**
   * See nsIUpdateService.idl
   */
  downloadUpdate: function AUS_downloadUpdate(update, background) {
    if (!update)
      throw Cr.NS_ERROR_NULL_POINTER;

    // Don't download the update if the update's version is less than the
    // current application's version or the update's version is the same as the
    // application's version and the build ID is the same as the application's
    // build ID.
    if (update.appVersion &&
        (Services.vc.compare(update.appVersion, Services.appinfo.version) < 0 ||
         update.buildID && update.buildID == Services.appinfo.appBuildID &&
         update.appVersion == Services.appinfo.version)) {
      LOG("UpdateService:downloadUpdate - canceling download of update since " +
          "it is for an earlier or same application version and build ID.\n" +
          "current application version: " + Services.appinfo.version + "\n" +
          "update application version : " + update.appVersion + "\n" +
          "current build ID: " + Services.appinfo.appBuildID + "\n" +
          "update build ID : " + update.buildID);
      cleanupActiveUpdate();
      return STATE_NONE;
    }

    // If a download request is in progress vs. a download ready to resume
    if (this.isDownloading) {
      if (update.isCompleteUpdate == this._downloader.isCompleteUpdate &&
          background == this._downloader.background) {
        LOG("UpdateService:downloadUpdate - no support for downloading more " +
            "than one update at a time");
        return readStatusFile(getUpdatesDir());
      }
      this._downloader.cancel();
    }
    // Set the previous application version prior to downloading the update.
    update.previousAppVersion = Services.appinfo.version;
    this._downloader = new Downloader(background, this);
    return this._downloader.downloadUpdate(update);
  },

  /**
   * See nsIUpdateService.idl
   */
  pauseDownload: function AUS_pauseDownload() {
    if (this.isDownloading) {
      this._downloader.cancel();
    } else if (this._retryTimer) {
      // Download status is still consider as 'downloading' during retry.
      // We need to cancel both retry and download at this stage.
      this._retryTimer.cancel();
      this._retryTimer = null;
      this._downloader.cancel();
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  getUpdatesDirectory: getUpdatesDir,

  /**
   * See nsIUpdateService.idl
   */
  get isDownloading() {
    return this._downloader && this._downloader.isBusy;
  },

  classID: UPDATESERVICE_CID,
  classInfo: XPCOMUtils.generateCI({classID: UPDATESERVICE_CID,
                                    contractID: UPDATESERVICE_CONTRACTID,
                                    interfaces: [Ci.nsIApplicationUpdateService,
                                                 Ci.nsITimerCallback,
                                                 Ci.nsIObserver],
                                    flags: Ci.nsIClassInfo.SINGLETON}),

  _xpcom_factory: UpdateServiceFactory,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIApplicationUpdateService,
                                         Ci.nsIUpdateCheckListener,
                                         Ci.nsITimerCallback,
                                         Ci.nsIObserver])
};

/**
 * A service to manage active and past updates.
 * @constructor
 */
function UpdateManager() {
  // Ensure the Active Update file is loaded
  var updates = this._loadXMLFileIntoArray(getUpdateFile(
                  [FILE_ACTIVE_UPDATE_XML]));
  if (updates.length > 0) {
    // Under some edgecases such as Windows system restore the active-update.xml
    // will contain a pending update without the status file which will return
    // STATE_NONE. To recover from this situation clean the updates dir and
    // rewrite the active-update.xml file without the broken update.
    if (readStatusFile(getUpdatesDir()) == STATE_NONE) {
      cleanUpUpdatesDir();
      this._writeUpdatesToXMLFile([], getUpdateFile([FILE_ACTIVE_UPDATE_XML]));
    } else
      this._activeUpdate = updates[0];
  }
}
UpdateManager.prototype = {
  /**
   * All previously downloaded and installed updates, as an array of nsIUpdate
   * objects.
   */
  _updates: null,

  /**
   * The current actively downloading/installing update, as a nsIUpdate object.
   */
  _activeUpdate: null,

  /**
   * Handle Observer Service notifications
   * @param   subject
   *          The subject of the notification
   * @param   topic
   *          The notification name
   * @param   data
   *          Additional data
   */
  observe: function UM_observe(subject, topic, data) {
    // Hack to be able to run and cleanup tests by reloading the update data.
    if (topic == "um-reload-update-data") {
      this._updates = this._loadXMLFileIntoArray(getUpdateFile(
                        [FILE_UPDATES_XML]));
      this._activeUpdate = null;
      var updates = this._loadXMLFileIntoArray(getUpdateFile(
                      [FILE_ACTIVE_UPDATE_XML]));
      if (updates.length > 0)
        this._activeUpdate = updates[0];
    }
  },

  /**
   * Loads an updates.xml formatted file into an array of nsIUpdate items.
   * @param   file
   *          A nsIFile for the updates.xml file
   * @return  The array of nsIUpdate items held in the file.
   */
  _loadXMLFileIntoArray: function UM__loadXMLFileIntoArray(file) {
    if (!file.exists()) {
      LOG("UpdateManager:_loadXMLFileIntoArray: XML file does not exist");
      return [];
    }

    var result = [];
    var fileStream = Cc["@mozilla.org/network/file-input-stream;1"].
                     createInstance(Ci.nsIFileInputStream);
    fileStream.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
    try {
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      var doc = parser.parseFromStream(fileStream, "UTF-8",
                                       fileStream.available(), "text/xml");

      const ELEMENT_NODE = Ci.nsIDOMNode.ELEMENT_NODE;
      var updateCount = doc.documentElement.childNodes.length;
      for (var i = 0; i < updateCount; ++i) {
        var updateElement = doc.documentElement.childNodes.item(i);
        if (updateElement.nodeType != ELEMENT_NODE ||
            updateElement.localName != "update")
          continue;

        updateElement.QueryInterface(Ci.nsIDOMElement);
        let update;
        try {
          update = new Update(updateElement);
        } catch (e) {
          LOG("UpdateManager:_loadXMLFileIntoArray - invalid update");
          continue;
        }
        result.push(update);
      }
    } catch (e) {
      LOG("UpdateManager:_loadXMLFileIntoArray - error constructing update " +
          "list. Exception: " + e);
    }
    fileStream.close();
    return result;
  },

  /**
   * Load the update manager, initializing state from state files.
   */
  _ensureUpdates: function UM__ensureUpdates() {
    if (!this._updates) {
      this._updates = this._loadXMLFileIntoArray(getUpdateFile(
                        [FILE_UPDATES_XML]));
      var activeUpdates = this._loadXMLFileIntoArray(getUpdateFile(
                            [FILE_ACTIVE_UPDATE_XML]));
      if (activeUpdates.length > 0)
        this._activeUpdate = activeUpdates[0];
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  getUpdateAt: function UM_getUpdateAt(index) {
    this._ensureUpdates();
    return this._updates[index];
  },

  /**
   * See nsIUpdateService.idl
   */
  get updateCount() {
    this._ensureUpdates();
    return this._updates.length;
  },

  /**
   * See nsIUpdateService.idl
   */
  get activeUpdate() {
    if (this._activeUpdate &&
        this._activeUpdate.channel != UpdateUtils.UpdateChannel) {
      LOG("UpdateManager:get activeUpdate - channel has changed, " +
          "reloading default preferences to workaround bug 802022");
      // Workaround to get distribution preferences loaded (Bug 774618). This
      // can be removed after bug 802022 is fixed.
      let prefSvc = Services.prefs.QueryInterface(Ci.nsIObserver);
      prefSvc.observe(null, "reload-default-prefs", null);
      if (this._activeUpdate.channel != UpdateUtils.UpdateChannel) {
        // User switched channels, clear out any old active updates and remove
        // partial downloads
        this._activeUpdate = null;
        this.saveUpdates();

        // Destroy the updates directory, since we're done with it.
        cleanUpUpdatesDir();
      }
    }
    return this._activeUpdate;
  },
  set activeUpdate(activeUpdate) {
    this._addUpdate(activeUpdate);
    this._activeUpdate = activeUpdate;
    if (!activeUpdate) {
      // If |activeUpdate| is null, we have updated both lists - the active list
      // and the history list, so we want to write both files.
      this.saveUpdates();
    } else
      this._writeUpdatesToXMLFile([this._activeUpdate],
                                  getUpdateFile([FILE_ACTIVE_UPDATE_XML]));
    return activeUpdate;
  },

  /**
   * Add an update to the Updates list. If the item already exists in the list,
   * replace the existing value with the new value.
   * @param   update
   *          The nsIUpdate object to add.
   */
  _addUpdate: function UM__addUpdate(update) {
    if (!update)
      return;
    this._ensureUpdates();
    // Only the latest update entry is checked so the the latest successful
    // step for an update is recorded and all failures are kept. This way
    // mutliple attempts to update for the same update are kept in the update
    // history.
    if (this._updates &&
        update.state != STATE_FAILED &&
        this._updates[0] &&
        this._updates[0].state != STATE_FAILED &&
        this._updates[0].appVersion == update.appVersion &&
        this._updates[0].buildID == update.buildID) {
      // Replace the existing entry with the new value, updating
      // all metadata.
      this._updates[0] = update;
      return;
    }
    // Otherwise add it to the front of the list.
    this._updates.unshift(update);
  },

  /**
   * Serializes an array of updates to an XML file
   * @param   updates
   *          An array of nsIUpdate objects
   * @param   file
   *          The nsIFile object to serialize to
   */
  _writeUpdatesToXMLFile: function UM__writeUpdatesToXMLFile(updates, file) {
    var fos = Cc["@mozilla.org/network/safe-file-output-stream;1"].
              createInstance(Ci.nsIFileOutputStream);
    var modeFlags = FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE |
                    FileUtils.MODE_TRUNCATE;
    if (!file.exists()) {
      file.create(Ci.nsILocalFile.NORMAL_FILE_TYPE, FileUtils.PERMS_FILE);
    }
    fos.init(file, modeFlags, FileUtils.PERMS_FILE, 0);

    try {
      var parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                   createInstance(Ci.nsIDOMParser);
      const EMPTY_UPDATES_DOCUMENT = "<?xml version=\"1.0\"?><updates xmlns=\"http://www.mozilla.org/2005/app-update\"></updates>";
      var doc = parser.parseFromString(EMPTY_UPDATES_DOCUMENT, "text/xml");

      for (var i = 0; i < updates.length; ++i) {
        // If appVersion isn't defined don't add the update. This happens when
        // cleaning up invalid updates (e.g. incorrect channel).
        if (updates[i] && updates[i].appVersion) {
          doc.documentElement.appendChild(updates[i].serialize(doc));
        }
      }

      var serializer = Cc["@mozilla.org/xmlextras/xmlserializer;1"].
                       createInstance(Ci.nsIDOMSerializer);
      serializer.serializeToStream(doc.documentElement, fos, null);
    } catch (e) {
    }

    FileUtils.closeSafeFileOutputStream(fos);
  },

  /**
   * See nsIUpdateService.idl
   */
  saveUpdates: function UM_saveUpdates() {
    this._writeUpdatesToXMLFile([this._activeUpdate],
                                getUpdateFile([FILE_ACTIVE_UPDATE_XML]));
    if (this._activeUpdate)
      this._addUpdate(this._activeUpdate);

    this._ensureUpdates();
    // Don't write updates that don't have a state to the updates.xml file.
    if (this._updates) {
      let updates = this._updates.slice();
      for (let i = updates.length - 1; i >= 0; --i) {
        let state = updates[i].state;
        if (state == STATE_NONE) {
          updates.splice(i, 1);
        }
      }

      this._writeUpdatesToXMLFile(updates.slice(0, 20),
                                  getUpdateFile([FILE_UPDATES_XML]));
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  refreshUpdateStatus: function UM_refreshUpdateStatus() {
    var update = this._activeUpdate;
    if (!update) {
      return;
    }
    var status = readStatusFile(getUpdatesDir());
    pingStateAndStatusCodes(update, false, status);
    var parts = status.split(":");
    update.state = parts[0];
    if (update.state == STATE_FAILED && parts[1]) {
      update.errorCode = parseInt(parts[1]);
    }
    let um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    // Save a copy of the active update's current state to the updates.xml so
    // it is possible to track failures.
    um.saveUpdates();

    // Rotate the update logs so the update log isn't removed if a complete
    // update is downloaded. By passing false the patch directory won't be
    // removed.
    cleanUpUpdatesDir(false);

    if (update.state == STATE_FAILED && parts[1]) {
      if (!handleUpdateFailure(update, parts[1])) {
        handleFallbackToCompleteUpdate(update, true);
      }

      update.QueryInterface(Ci.nsIWritablePropertyBag);
      update.setProperty("stagingFailed", "true");
    }
    if (update.state == STATE_APPLIED && shouldUseService()) {
      writeStatusFile(getUpdatesDir(), update.state = STATE_APPLIED_SERVICE);
    }

    // Send an observer notification which the app update doorhanger uses to
    // display a restart notification
    LOG("UpdateManager:refreshUpdateStatus - Notifying observers that " +
        "the update was staged. topic: update-staged, status: " + update.state);
    Services.obs.notifyObservers(update, "update-staged", update.state);

    // Only prompt when the UI isn't already open.
    let windowType = getPref("getCharPref", PREF_APP_UPDATE_ALTWINDOWTYPE, null);
    if (Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME) ||
        windowType && Services.wm.getMostRecentWindow(windowType)) {
      return;
    }

    if (update.state == STATE_APPLIED ||
        update.state == STATE_APPLIED_SERVICE ||
        update.state == STATE_PENDING ||
        update.state == STATE_PENDING_SERVICE ||
        update.state == STATE_PENDING_ELEVATE) {
      // Notify the user that an update has been staged and is ready for
      // installation (i.e. that they should restart the application).
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateDownloaded(update, true);
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  elevationOptedIn: function UM_elevationOptedIn() {
    // The user has been been made aware that the update requires elevation.
    let update = this._activeUpdate;
    if (!update) {
      return;
    }
    let status = readStatusFile(getUpdatesDir());
    let parts = status.split(":");
    update.state = parts[0];
    if (update.state == STATE_PENDING_ELEVATE) {
      // Proceed with the pending update.
      // Note: STATE_PENDING_ELEVATE stands for "pending user's approval to
      // proceed with an elevated update". As long as we see this state, we will
      // notify the user of the availability of an update that requires
      // elevation. |elevationOptedIn| (this function) is called when the user
      // gives us approval to proceed, so we want to switch to STATE_PENDING.
      // The updater then detects whether or not elevation is required and
      // displays the elevation prompt if necessary. This last step does not
      // depend on the state in the status file.
      writeStatusFile(getUpdatesDir(), STATE_PENDING);
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  cleanupActiveUpdate: function UM_cleanupActiveUpdate() {
    cleanupActiveUpdate();
  },

  classID: Components.ID("{093C2356-4843-4C65-8709-D7DBCBBE7DFB}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateManager, Ci.nsIObserver])
};

/**
 * Checker
 * Checks for new Updates
 * @constructor
 */
function Checker() {
}
Checker.prototype = {
  /**
   * The XMLHttpRequest object that performs the connection.
   */
  _request: null,

  /**
   * The nsIUpdateCheckListener callback
   */
  _callback: null,

  /**
   * The URL of the update service XML file to connect to that contains details
   * about available updates.
   */
  getUpdateURL: async function UC_getUpdateURL(force) {
    this._forced = force;

    let url = Services.prefs.getDefaultBranch(null).
              getCharPref(PREF_APP_UPDATE_URL, "");

    if (!url) {
      LOG("Checker:getUpdateURL - update URL not defined");
      return null;
    }

    url = await UpdateUtils.formatUpdateURL(url);

    if (force) {
      url += (url.indexOf("?") != -1 ? "&" : "?") + "force=1";
    }

    LOG("Checker:getUpdateURL - update URL: " + url);
    return url;
  },

  /**
   * See nsIUpdateService.idl
   */
  checkForUpdates: function UC_checkForUpdates(listener, force) {
    LOG("Checker: checkForUpdates, force: " + force);
    if (!listener) {
      throw Cr.NS_ERROR_NULL_POINTER;
    }

    if (!this.enabled && !force) {
      return;
    }

    this.getUpdateURL(force).then(url => {
      if (!url) {
        return;
      }

      this._request = new XMLHttpRequest();
      this._request.open("GET", url, true);
      this._request.channel.notificationCallbacks = new gCertUtils.BadCertHandler(false);
      // Prevent the request from reading from the cache.
      this._request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
      // Prevent the request from writing to the cache.
      this._request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
      // Disable cutting edge features, like TLS 1.3, where middleboxes might brick us
      this._request.channel.QueryInterface(Ci.nsIHttpChannelInternal).beConservative = true;

      this._request.overrideMimeType("text/xml");
      // The Cache-Control header is only interpreted by proxies and the
      // final destination. It does not help if a resource is already
      // cached locally.
      this._request.setRequestHeader("Cache-Control", "no-cache");
      // HTTP/1.0 servers might not implement Cache-Control and
      // might only implement Pragma: no-cache
      this._request.setRequestHeader("Pragma", "no-cache");

      var self = this;
      this._request.addEventListener("error", function(event) { self.onError(event); });
      this._request.addEventListener("load", function(event) { self.onLoad(event); });

      LOG("Checker:checkForUpdates - sending request to: " + url);
      this._request.send(null);

      this._callback = listener;
    });
  },

  /**
   * Returns an array of nsIUpdate objects discovered by the update check.
   * @throws if the XML document element node name is not updates.
   */
  get _updates() {
    var updatesElement = this._request.responseXML.documentElement;
    if (!updatesElement) {
      LOG("Checker:_updates get - empty updates document?!");
      return [];
    }

    if (updatesElement.nodeName != "updates") {
      LOG("Checker:_updates get - unexpected node name!");
      throw new Error("Unexpected node name, expected: updates, got: " +
                      updatesElement.nodeName);
    }

    const ELEMENT_NODE = Ci.nsIDOMNode.ELEMENT_NODE;
    var updates = [];
    for (var i = 0; i < updatesElement.childNodes.length; ++i) {
      var updateElement = updatesElement.childNodes.item(i);
      if (updateElement.nodeType != ELEMENT_NODE ||
          updateElement.localName != "update")
        continue;

      updateElement.QueryInterface(Ci.nsIDOMElement);
      let update;
      try {
        update = new Update(updateElement);
      } catch (e) {
        LOG("Checker:_updates get - invalid <update/>, ignoring...");
        continue;
      }
      update.serviceURL = this._request.responseURL;
      update.channel = UpdateUtils.UpdateChannel;
      updates.push(update);
    }

    return updates;
  },

  /**
   * Returns the status code for the XMLHttpRequest
   */
  _getChannelStatus: function UC__getChannelStatus(request) {
    var status = 0;
    try {
      status = request.status;
    } catch (e) {
    }

    if (status == 0)
      status = request.channel.QueryInterface(Ci.nsIRequest).status;
    return status;
  },

  _isHttpStatusCode: function UC__isHttpStatusCode(status) {
    return status >= 100 && status <= 599;
  },

  /**
   * The XMLHttpRequest succeeded and the document was loaded.
   * @param   event
   *          The nsIDOMEvent for the load
   */
  onLoad: function UC_onLoad(event) {
    LOG("Checker:onLoad - request completed downloading document");

    try {
      // Analyze the resulting DOM and determine the set of updates.
      var updates = this._updates;
      LOG("Checker:onLoad - number of updates available: " + updates.length);

      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS)) {
        Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);
      }

      // Tell the callback about the updates
      this._callback.onCheckComplete(event.target, updates, updates.length);
    } catch (e) {
      LOG("Checker:onLoad - there was a problem checking for updates. " +
          "Exception: " + e);
      var request = event.target;
      var status = this._getChannelStatus(request);
      LOG("Checker:onLoad - request.status: " + status);
      var update = new Update(null);
      update.errorCode = status;
      update.statusText = getStatusTextFromCode(status, 404);

      if (this._isHttpStatusCode(status)) {
        update.errorCode = HTTP_ERROR_OFFSET + status;
      }

      this._callback.onError(request, update);
    }

    this._callback = null;
    this._request = null;
  },

  /**
   * There was an error of some kind during the XMLHttpRequest
   * @param   event
   *          The nsIDOMEvent for the error
   */
  onError: function UC_onError(event) {
    var request = event.target;
    var status = this._getChannelStatus(request);
    LOG("Checker:onError - request.status: " + status);

    // If we can't find an error string specific to this status code,
    // just use the 200 message from above, which means everything
    // "looks" fine but there was probably an XML error or a bogus file.
    var update = new Update(null);
    update.errorCode = status;
    update.statusText = getStatusTextFromCode(status, 200);

    if (status == Cr.NS_ERROR_OFFLINE) {
      // We use a separate constant here because nsIUpdate.errorCode is signed
      update.errorCode = NETWORK_ERROR_OFFLINE;
    } else if (this._isHttpStatusCode(status)) {
      update.errorCode = HTTP_ERROR_OFFSET + status;
    }

    this._callback.onError(request, update);
    this._request = null;
  },

  /**
   * Whether or not we are allowed to do update checking.
   */
  _enabled: true,
  get enabled() {
    return getPref("getBoolPref", PREF_APP_UPDATE_ENABLED, true) &&
           gCanCheckForUpdates && hasUpdateMutex() && this._enabled;
  },

  /**
   * See nsIUpdateService.idl
   */
  stopChecking: function UC_stopChecking(duration) {
    // Always stop the current check
    if (this._request)
      this._request.abort();

    switch (duration) {
      case Ci.nsIUpdateChecker.CURRENT_SESSION:
        this._enabled = false;
        break;
      case Ci.nsIUpdateChecker.ANY_CHECKS:
        this._enabled = false;
        Services.prefs.setBoolPref(PREF_APP_UPDATE_ENABLED, this._enabled);
        break;
    }

    this._callback = null;
  },

  classID: Components.ID("{898CDC9B-E43F-422F-9CC4-2F6291B415A3}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateChecker])
};

/**
 * Manages the download of updates
 * @param   background
 *          Whether or not this downloader is operating in background
 *          update mode.
 * @param   updateService
 *          The update service that created this downloader.
 * @constructor
 */
function Downloader(background, updateService) {
  LOG("Creating Downloader");
  this.background = background;
  this.updateService = updateService;
}
Downloader.prototype = {
  /**
   * The nsIUpdatePatch that we are downloading
   */
  _patch: null,

  /**
   * The nsIUpdate that we are downloading
   */
  _update: null,

  /**
   * The nsIIncrementalDownload object handling the download
   */
  _request: null,

  /**
   * Whether or not the update being downloaded is a complete replacement of
   * the user's existing installation or a patch representing the difference
   * between the new version and the previous version.
   */
  isCompleteUpdate: null,

  /**
   * Cancels the active download.
   */
  cancel: function Downloader_cancel(cancelError) {
    LOG("Downloader: cancel");
    if (cancelError === undefined) {
      cancelError = Cr.NS_BINDING_ABORTED;
    }
    if (this._request && this._request instanceof Ci.nsIRequest) {
      this._request.cancel(cancelError);
    }
  },

  /**
   * Whether or not a patch has been downloaded and staged for installation.
   */
  get patchIsStaged() {
    var readState = readStatusFile(getUpdatesDir());
    // Note that if we decide to download and apply new updates after another
    // update has been successfully applied in the background, we need to stop
    // checking for the APPLIED state here.
    return readState == STATE_PENDING || readState == STATE_PENDING_SERVICE ||
           readState == STATE_PENDING_ELEVATE ||
           readState == STATE_APPLIED || readState == STATE_APPLIED_SERVICE;
  },

  /**
   * Verify the downloaded file.  We assume that the download is complete at
   * this point.
   */
  _verifyDownload: function Downloader__verifyDownload() {
    LOG("Downloader:_verifyDownload called");
    if (!this._request) {
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_VERIFY_NO_REQUEST);
      return false;
    }

    let destination = this._request.destination;

    // Ensure that the file size matches the expected file size.
    if (destination.fileSize != this._patch.size) {
      LOG("Downloader:_verifyDownload downloaded size != expected size.");
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_VERIFY_PATCH_SIZE_NOT_EQUAL);
      return false;
    }

    LOG("Downloader:_verifyDownload downloaded size == expected size.");
    return true;
  },

  /**
   * Select the patch to use given the current state of updateDir and the given
   * set of update patches.
   * @param   update
   *          A nsIUpdate object to select a patch from
   * @param   updateDir
   *          A nsIFile representing the update directory
   * @return  A nsIUpdatePatch object to download
   */
  _selectPatch: function Downloader__selectPatch(update, updateDir) {
    // Given an update to download, we will always try to download the patch
    // for a partial update over the patch for a full update.

    /**
     * Return the first UpdatePatch with the given type.
     * @param   type
     *          The type of the patch ("complete" or "partial")
     * @return  A nsIUpdatePatch object matching the type specified
     */
    function getPatchOfType(type) {
      for (var i = 0; i < update.patchCount; ++i) {
        var patch = update.getPatchAt(i);
        if (patch && patch.type == type)
          return patch;
      }
      return null;
    }

    // Look to see if any of the patches in the Update object has been
    // pre-selected for download, otherwise we must figure out which one
    // to select ourselves.
    var selectedPatch = update.selectedPatch;

    var state = readStatusFile(updateDir);

    // If this is a patch that we know about, then select it.  If it is a patch
    // that we do not know about, then remove it and use our default logic.
    var useComplete = false;
    if (selectedPatch) {
      LOG("Downloader:_selectPatch - found existing patch with state: " +
          state);
      if (state == STATE_DOWNLOADING) {
        LOG("Downloader:_selectPatch - resuming download");
        return selectedPatch;
      }
      if (state == STATE_PENDING || state == STATE_PENDING_SERVICE ||
          state == STATE_PENDING_ELEVATE || state == STATE_APPLIED ||
          state == STATE_APPLIED_SERVICE) {
        LOG("Downloader:_selectPatch - already downloaded");
        return null;
      }

      if (update && selectedPatch.type == "complete") {
        // This is a pretty fatal error.  Just bail.
        LOG("Downloader:_selectPatch - failed to apply complete patch!");
        writeStatusFile(updateDir, STATE_NONE);
        writeVersionFile(getUpdatesDir(), null);
        return null;
      }

      // Something went wrong when we tried to apply the previous patch.
      // Try the complete patch next time.
      useComplete = true;
      selectedPatch = null;
    }

    // If we were not able to discover an update from a previous download, we
    // select the best patch from the given set.
    var partialPatch = getPatchOfType("partial");
    if (!useComplete)
      selectedPatch = partialPatch;
    if (!selectedPatch) {
      if (partialPatch)
        partialPatch.selected = false;
      selectedPatch = getPatchOfType("complete");
    }

    // Restore the updateDir since we may have deleted it.
    updateDir = getUpdatesDir();

    // if update only contains a partial patch, selectedPatch == null here if
    // the partial patch has been attempted and fails and we're trying to get a
    // complete patch
    if (selectedPatch)
      selectedPatch.selected = true;

    update.isCompleteUpdate = useComplete;

    // Reset the Active Update object on the Update Manager and flush the
    // Active Update DB.
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.activeUpdate = update;

    return selectedPatch;
  },

  /**
   * Whether or not we are currently downloading something.
   */
  get isBusy() {
    return this._request != null;
  },

  /**
   * Download and stage the given update.
   * @param   update
   *          A nsIUpdate object to download a patch for. Cannot be null.
   */
  downloadUpdate: function Downloader_downloadUpdate(update) {
    LOG("UpdateService:_downloadUpdate");
    if (!update) {
      AUSTLMY.pingDownloadCode(undefined, AUSTLMY.DWNLD_ERR_NO_UPDATE);
      throw Cr.NS_ERROR_NULL_POINTER;
    }

    var updateDir = getUpdatesDir();

    this._update = update;

    // This function may return null, which indicates that there are no patches
    // to download.
    this._patch = this._selectPatch(update, updateDir);
    if (!this._patch) {
      LOG("Downloader:downloadUpdate - no patch to download");
      AUSTLMY.pingDownloadCode(undefined, AUSTLMY.DWNLD_ERR_NO_UPDATE_PATCH);
      return readStatusFile(updateDir);
    }
    this.isCompleteUpdate = this._patch.type == "complete";

    let patchFile = getUpdatesDir().clone();
    patchFile.append(FILE_UPDATE_MAR);
    update.QueryInterface(Ci.nsIPropertyBag);
    let interval = this.background ? update.getProperty("backgroundInterval")
                                   : DOWNLOAD_FOREGROUND_INTERVAL;

    LOG("Downloader:downloadUpdate - url: " + this._patch.URL + ", path: " +
        patchFile.path + ", interval: " + interval);
    var uri = Services.io.newURI(this._patch.URL);

    this._request = Cc["@mozilla.org/network/incremental-download;1"].
                    createInstance(Ci.nsIIncrementalDownload);
    this._request.init(uri, patchFile, DOWNLOAD_CHUNK_SIZE, interval);
    this._request.start(this, null);

    writeStatusFile(updateDir, STATE_DOWNLOADING);
    this._patch.QueryInterface(Ci.nsIWritablePropertyBag);
    this._patch.state = STATE_DOWNLOADING;
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.saveUpdates();
    return STATE_DOWNLOADING;
  },

  /**
   * An array of download listeners to notify when we receive
   * nsIRequestObserver or nsIProgressEventSink method calls.
   */
  _listeners: [],

  /**
   * Adds a listener to the download process
   * @param   listener
   *          A download listener, implementing nsIRequestObserver and
   *          nsIProgressEventSink
   */
  addDownloadListener: function Downloader_addDownloadListener(listener) {
    for (var i = 0; i < this._listeners.length; ++i) {
      if (this._listeners[i] == listener)
        return;
    }
    this._listeners.push(listener);
  },

  /**
   * Removes a download listener
   * @param   listener
   *          The listener to remove.
   */
  removeDownloadListener: function Downloader_removeDownloadListener(listener) {
    for (var i = 0; i < this._listeners.length; ++i) {
      if (this._listeners[i] == listener) {
        this._listeners.splice(i, 1);
        return;
      }
    }
  },

  /**
   * When the async request begins
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   */
  onStartRequest: function Downloader_onStartRequest(request, context) {
    if (request instanceof Ci.nsIIncrementalDownload)
      LOG("Downloader:onStartRequest - original URI spec: " + request.URI.spec +
          ", final URI spec: " + request.finalURI.spec);
    // Always set finalURL in onStartRequest since it can change.
    this._patch.finalURL = request.finalURI.spec;
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    um.saveUpdates();

    var listeners = this._listeners.concat();
    var listenerCount = listeners.length;
    for (var i = 0; i < listenerCount; ++i)
      listeners[i].onStartRequest(request, context);
  },

  /**
   * When new data has been downloaded
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   progress
   *          The current number of bytes transferred
   * @param   maxProgress
   *          The total number of bytes that must be transferred
   */
  onProgress: function Downloader_onProgress(request, context, progress,
                                             maxProgress) {
    LOG("Downloader:onProgress - progress: " + progress + "/" + maxProgress);

    if (progress > this._patch.size) {
      LOG("Downloader:onProgress - progress: " + progress +
          " is higher than patch size: " + this._patch.size);
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_PATCH_SIZE_LARGER);
      this.cancel(Cr.NS_ERROR_UNEXPECTED);
      return;
    }

    if (maxProgress != this._patch.size) {
      LOG("Downloader:onProgress - maxProgress: " + maxProgress +
          " is not equal to expected patch size: " + this._patch.size);
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_ERR_PATCH_SIZE_NOT_EQUAL);
      this.cancel(Cr.NS_ERROR_UNEXPECTED);
      return;
    }

    var listeners = this._listeners.concat();
    var listenerCount = listeners.length;
    for (var i = 0; i < listenerCount; ++i) {
      var listener = listeners[i];
      if (listener instanceof Ci.nsIProgressEventSink)
        listener.onProgress(request, context, progress, maxProgress);
    }
    this.updateService._consecutiveSocketErrors = 0;
  },

  /**
   * When we have new status text
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   status
   *          A status code
   * @param   statusText
   *          Human readable version of |status|
   */
  onStatus: function Downloader_onStatus(request, context, status, statusText) {
    LOG("Downloader:onStatus - status: " + status + ", statusText: " +
        statusText);

    var listeners = this._listeners.concat();
    var listenerCount = listeners.length;
    for (var i = 0; i < listenerCount; ++i) {
      var listener = listeners[i];
      if (listener instanceof Ci.nsIProgressEventSink)
        listener.onStatus(request, context, status, statusText);
    }
  },

  /**
   * When data transfer ceases
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   context
   *          Additional data
   * @param   status
   *          Status code containing the reason for the cessation.
   */
  onStopRequest: function Downloader_onStopRequest(request, context, status) {
    if (request instanceof Ci.nsIIncrementalDownload)
      LOG("Downloader:onStopRequest - original URI spec: " + request.URI.spec +
          ", final URI spec: " + request.finalURI.spec + ", status: " + status);

    // XXX ehsan shouldShowPrompt should always be false here.
    // But what happens when there is already a UI showing?
    var state = this._patch.state;
    var shouldShowPrompt = false;
    var shouldRegisterOnlineObserver = false;
    var shouldRetrySoon = false;
    var deleteActiveUpdate = false;
    var retryTimeout = getPref("getIntPref", PREF_APP_UPDATE_SOCKET_RETRYTIMEOUT,
                               DEFAULT_SOCKET_RETRYTIMEOUT);
    // Prevent the preference from setting a value greater than 10000.
    retryTimeout = Math.min(retryTimeout, 10000);
    var maxFail = getPref("getIntPref", PREF_APP_UPDATE_SOCKET_MAXERRORS,
                          DEFAULT_SOCKET_MAX_ERRORS);
    // Prevent the preference from setting a value greater than 20.
    maxFail = Math.min(maxFail, 20);
    LOG("Downloader:onStopRequest - status: " + status + ", " +
        "current fail: " + this.updateService._consecutiveSocketErrors + ", " +
        "max fail: " + maxFail + ", " +
        "retryTimeout: " + retryTimeout);
    if (Components.isSuccessCode(status)) {
      if (this._verifyDownload()) {
        if (shouldUseService()) {
          state = STATE_PENDING_SERVICE;
        } else if (getElevationRequired()) {
          state = STATE_PENDING_ELEVATE;
        } else {
          state = STATE_PENDING;
        }
        if (this.background) {
          shouldShowPrompt = !getCanStageUpdates();
        }
        AUSTLMY.pingDownloadCode(this.isCompleteUpdate, AUSTLMY.DWNLD_SUCCESS);

        // Tell the updater.exe we're ready to apply.
        writeStatusFile(getUpdatesDir(), state);
        writeVersionFile(getUpdatesDir(), this._update.appVersion);
        this._update.installDate = (new Date()).getTime();
        this._update.statusText = gUpdateBundle.GetStringFromName("installPending");
        Services.prefs.setIntPref(PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS, 0);
      } else {
        LOG("Downloader:onStopRequest - download verification failed");
        state = STATE_DOWNLOAD_FAILED;
        status = Cr.NS_ERROR_CORRUPTED_CONTENT;

        // Yes, this code is a string.
        const vfCode = "verification_failed";
        var message = getStatusTextFromCode(vfCode, vfCode);
        this._update.statusText = message;

        if (this._update.isCompleteUpdate || this._update.patchCount != 2)
          deleteActiveUpdate = true;

        // Destroy the updates directory, since we're done with it.
        cleanUpUpdatesDir();
      }
    } else if (status == Cr.NS_ERROR_OFFLINE) {
      // Register an online observer to try again.
      // The online observer will continue the incremental download by
      // calling downloadUpdate on the active update which continues
      // downloading the file from where it was.
      LOG("Downloader:onStopRequest - offline, register online observer: true");
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate,
                               AUSTLMY.DWNLD_RETRY_OFFLINE);
      shouldRegisterOnlineObserver = true;
      deleteActiveUpdate = false;
    // Each of NS_ERROR_NET_TIMEOUT, ERROR_CONNECTION_REFUSED,
    // NS_ERROR_NET_RESET and NS_ERROR_DOCUMENT_NOT_CACHED can be returned
    // when disconnecting the internet while a download of a MAR is in
    // progress.  There may be others but I have not encountered them during
    // testing.
    } else if ((status == Cr.NS_ERROR_NET_TIMEOUT ||
                status == Cr.NS_ERROR_CONNECTION_REFUSED ||
                status == Cr.NS_ERROR_NET_RESET ||
                status == Cr.NS_ERROR_DOCUMENT_NOT_CACHED) &&
               this.updateService._consecutiveSocketErrors < maxFail) {
      LOG("Downloader:onStopRequest - socket error, shouldRetrySoon: true");
      let dwnldCode = AUSTLMY.DWNLD_RETRY_CONNECTION_REFUSED;
      if (status == Cr.NS_ERROR_NET_TIMEOUT) {
        dwnldCode = AUSTLMY.DWNLD_RETRY_NET_TIMEOUT;
      } else if (status == Cr.NS_ERROR_NET_RESET) {
        dwnldCode = AUSTLMY.DWNLD_RETRY_NET_RESET;
      } else if (status == Cr.NS_ERROR_DOCUMENT_NOT_CACHED) {
        dwnldCode = AUSTLMY.DWNLD_ERR_DOCUMENT_NOT_CACHED;
      }
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate, dwnldCode);
      shouldRetrySoon = true;
      deleteActiveUpdate = false;
    } else if (status != Cr.NS_BINDING_ABORTED &&
               status != Cr.NS_ERROR_ABORT) {
      LOG("Downloader:onStopRequest - non-verification failure");
      let dwnldCode = AUSTLMY.DWNLD_ERR_BINDING_ABORTED;
      if (status == Cr.NS_ERROR_ABORT) {
        dwnldCode = AUSTLMY.DWNLD_ERR_ABORT;
      }
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate, dwnldCode);

      // Some sort of other failure, log this in the |statusText| property
      state = STATE_DOWNLOAD_FAILED;

      // XXXben - if |request| (The Incremental Download) provided a means
      // for accessing the http channel we could do more here.

      this._update.statusText = getStatusTextFromCode(status,
                                                      Cr.NS_BINDING_FAILED);

      // Destroy the updates directory, since we're done with it.
      cleanUpUpdatesDir();

      deleteActiveUpdate = true;
    }
    LOG("Downloader:onStopRequest - setting state to: " + state);
    this._patch.state = state;
    var um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    if (deleteActiveUpdate) {
      this._update.installDate = (new Date()).getTime();
      um.activeUpdate = null;
    } else if (um.activeUpdate) {
      um.activeUpdate.state = state;
    }
    um.saveUpdates();

    // Only notify listeners about the stopped state if we
    // aren't handling an internal retry.
    if (!shouldRetrySoon && !shouldRegisterOnlineObserver) {
      var listeners = this._listeners.concat();
      var listenerCount = listeners.length;
      for (var i = 0; i < listenerCount; ++i) {
        listeners[i].onStopRequest(request, context, status);
      }
    }

    this._request = null;

    if (state == STATE_DOWNLOAD_FAILED) {
      var allFailed = true;
      // Check if there is a complete update patch that can be downloaded.
      if (!this._update.isCompleteUpdate && this._update.patchCount == 2) {
        LOG("Downloader:onStopRequest - verification of patch failed, " +
            "downloading complete update patch");
        this._update.isCompleteUpdate = true;
        let updateStatus = this.downloadUpdate(this._update);

        if (updateStatus == STATE_NONE) {
          cleanupActiveUpdate();
        } else {
          allFailed = false;
        }
      }

      if (allFailed) {
        if (getPref("getBoolPref", PREF_APP_UPDATE_DOORHANGER, false)) {
          let downloadAttempts = getPref("getIntPref", PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS, 0);
          downloadAttempts++;
          Services.prefs.setIntPref(PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS, downloadAttempts);
          let maxAttempts = Math.min(getPref("getIntPref", PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS, 2), 10);

          if (downloadAttempts > maxAttempts) {
            LOG("Downloader:onStopRequest - notifying observers of error. " +
                "topic: update-error, status: download-attempts-exceeded, " +
                "downloadAttempts: " + downloadAttempts + " " +
                "maxAttempts: " + maxAttempts);
            Services.obs.notifyObservers(this._update, "update-error", "download-attempts-exceeded");
          } else {
            this._update.selectedPatch.selected = false;
            LOG("Downloader:onStopRequest - notifying observers of error. " +
                "topic: update-error, status: download-attempt-failed");
            Services.obs.notifyObservers(this._update, "update-error", "download-attempt-failed");
          }
        }

        // If the update UI is not open (e.g. the user closed the window while
        // downloading) and if at any point this was a foreground download
        // notify the user about the error. If the update was a background
        // update there is no notification since the user won't be expecting it.
        if (!Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME)) {
          this._update.QueryInterface(Ci.nsIWritablePropertyBag);
          if (this._update.getProperty("foregroundDownload") == "true") {
            let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                           createInstance(Ci.nsIUpdatePrompt);
            prompter.showUpdateError(this._update);
          }
        }

        // Prevent leaking the update object (bug 454964).
        this._update = null;
      }
      // A complete download has been initiated or the failure was handled.
      return;
    }

    if (state == STATE_PENDING || state == STATE_PENDING_SERVICE ||
        state == STATE_PENDING_ELEVATE) {
      if (getCanStageUpdates()) {
        LOG("Downloader:onStopRequest - attempting to stage update: " +
            this._update.name);

        // Initiate the update in the background
        try {
          Cc["@mozilla.org/updates/update-processor;1"].
            createInstance(Ci.nsIUpdateProcessor).
            processUpdate(this._update);
        } catch (e) {
          // Fail gracefully in case the application does not support the update
          // processor service.
          LOG("Downloader:onStopRequest - failed to stage update. Exception: " +
              e);
          if (this.background) {
            shouldShowPrompt = true;
          }
        }
      }
    }

    // Do this after *everything* else, since it will likely cause the app
    // to shut down.
    if (shouldShowPrompt) {
      // Notify the user that an update has been downloaded and is ready for
      // installation (i.e. that they should restart the application). We do
      // not notify on failed update attempts.
      let prompter = Cc["@mozilla.org/updates/update-prompt;1"].
                     createInstance(Ci.nsIUpdatePrompt);
      prompter.showUpdateDownloaded(this._update, true);
    }

    if (shouldRegisterOnlineObserver) {
      LOG("Downloader:onStopRequest - Registering online observer");
      this.updateService._registerOnlineObserver();
    } else if (shouldRetrySoon) {
      LOG("Downloader:onStopRequest - Retrying soon");
      this.updateService._consecutiveSocketErrors++;
      if (this.updateService._retryTimer) {
        this.updateService._retryTimer.cancel();
      }
      this.updateService._retryTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this.updateService._retryTimer.initWithCallback(function() {
        this._attemptResume();
      }.bind(this.updateService), retryTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
    } else {
      // Prevent leaking the update object (bug 454964)
      this._update = null;
    }
  },

  /**
   * See nsIInterfaceRequestor.idl
   */
  getInterface: function Downloader_getInterface(iid) {
    // The network request may require proxy authentication, so provide the
    // default nsIAuthPrompt if requested.
    if (iid.equals(Ci.nsIAuthPrompt)) {
      var prompt = Cc["@mozilla.org/network/default-auth-prompt;1"].
                   createInstance();
      return prompt.QueryInterface(iid);
    }
    throw Cr.NS_NOINTERFACE;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIRequestObserver,
                                         Ci.nsIProgressEventSink,
                                         Ci.nsIInterfaceRequestor])
};

/**
 * UpdatePrompt
 * An object which can prompt the user with information about updates, request
 * action, etc. Embedding clients can override this component with one that
 * invokes a native front end.
 * @constructor
 */
function UpdatePrompt() {
}
UpdatePrompt.prototype = {
  /**
   * See nsIUpdateService.idl
   */
  checkForUpdates: function UP_checkForUpdates() {
    if (this._getAltUpdateWindow())
      return;

    this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null, UPDATE_WINDOW_NAME,
                 null, null);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateAvailable: function UP_showUpdateAvailable(update) {
    if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false) ||
        getPref("getBoolPref", PREF_APP_UPDATE_DOORHANGER, false) ||
        this._getUpdateWindow() || this._getAltUpdateWindow()) {
      return;
    }

    this._showUnobtrusiveUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                           UPDATE_WINDOW_NAME, "updatesavailable", update);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateDownloaded: function UP_showUpdateDownloaded(update, background) {
    if (background && getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false)) {
      return;
    }

    // Trigger the display of the hamburger doorhanger.
    LOG("showUpdateDownloaded - Notifying observers that " +
        "an update was downloaded. topic: update-downloaded, status: " + update.state);
    Services.obs.notifyObservers(update, "update-downloaded", update.state);

    if (getPref("getBoolPref", PREF_APP_UPDATE_DOORHANGER, false)) {
      return;
    }

    if (this._getAltUpdateWindow())
      return;

    if (background) {
      this._showUnobtrusiveUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                              UPDATE_WINDOW_NAME, "finishedBackground", update);
    } else {
      this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                   UPDATE_WINDOW_NAME, "finishedBackground", update);
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateError: function UP_showUpdateError(update) {
    if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false) ||
        getPref("getBoolPref", PREF_APP_UPDATE_DOORHANGER, false) ||
        this._getAltUpdateWindow())
      return;

    // In some cases, we want to just show a simple alert dialog.
    if (update.state == STATE_FAILED &&
        WRITE_ERRORS.includes(update.errorCode)) {
      var title = gUpdateBundle.GetStringFromName("updaterIOErrorTitle");
      var text = gUpdateBundle.formatStringFromName("updaterIOErrorMsg",
                                                    [Services.appinfo.name,
                                                     Services.appinfo.name], 2);
      Services.ww.getNewPrompter(null).alert(title, text);
      return;
    }

    if (update.errorCode == BACKGROUNDCHECK_MULTIPLE_FAILURES) {
      this._showUIWhenIdle(null, URI_UPDATE_PROMPT_DIALOG, null,
                           UPDATE_WINDOW_NAME, null, update);
      return;
    }

    this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null, UPDATE_WINDOW_NAME,
                 "errors", update);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateHistory: function UP_showUpdateHistory(parent) {
    this._showUI(parent, URI_UPDATE_HISTORY_DIALOG, "modal,dialog=yes",
                 "Update:History", null, null);
  },

  /**
   * See nsIUpdateService.idl
   */
  showUpdateElevationRequired: function UP_showUpdateElevationRequired() {
    if (getPref("getBoolPref", PREF_APP_UPDATE_SILENT, false) ||
        getPref("getBoolPref", PREF_APP_UPDATE_DOORHANGER, false) ||
        this._getAltUpdateWindow()) {
      return;
    }

    let um = Cc["@mozilla.org/updates/update-manager;1"].
             getService(Ci.nsIUpdateManager);
    this._showUI(null, URI_UPDATE_PROMPT_DIALOG, null,
                 UPDATE_WINDOW_NAME, "finishedBackground", um.activeUpdate);
  },

  /**
   * Returns the update window if present.
   */
  _getUpdateWindow: function UP__getUpdateWindow() {
    return Services.wm.getMostRecentWindow(UPDATE_WINDOW_NAME);
  },

  /**
   * Returns an alternative update window if present. When a window with this
   * windowtype is open the application update service won't open the normal
   * application update user interface window.
   */
  _getAltUpdateWindow: function UP__getAltUpdateWindow() {
    let windowType = getPref("getCharPref", PREF_APP_UPDATE_ALTWINDOWTYPE, null);
    if (!windowType)
      return null;
    return Services.wm.getMostRecentWindow(windowType);
  },

  /**
   * Display the update UI after the prompt wait time has elapsed.
   * @param   parent
   *          A parent window, can be null
   * @param   uri
   *          The URI string of the dialog to show
   * @param   name
   *          The Window Name of the dialog to show, in case it is already open
   *          and can merely be focused
   * @param   page
   *          The page of the wizard to be displayed, if one is already open.
   * @param   update
   *          An update to pass to the UI in the window arguments.
   *          Can be null
   */
  _showUnobtrusiveUI: function UP__showUnobUI(parent, uri, features, name, page,
                                              update) {
    var observer = {
      updatePrompt: this,
      service: null,
      timer: null,
      notify() {
        // the user hasn't restarted yet => prompt when idle
        this.service.removeObserver(this, "quit-application");
        // If the update window is already open skip showing the UI
        if (this.updatePrompt._getUpdateWindow())
          return;
        this.updatePrompt._showUIWhenIdle(parent, uri, features, name, page, update);
      },
      observe(aSubject, aTopic, aData) {
        switch (aTopic) {
          case "quit-application":
            if (this.timer)
              this.timer.cancel();
            this.service.removeObserver(this, "quit-application");
            break;
        }
      }
    };

    // bug 534090 - show the UI for update available notifications when the
    // the system has been idle for at least IDLE_TIME.
    if (page == "updatesavailable") {
      var idleService = Cc["@mozilla.org/widget/idleservice;1"].
                        getService(Ci.nsIIdleService);
      // Don't allow the preference to set a value greater than 600 seconds for the idle time.
      const IDLE_TIME = Math.min(getPref("getIntPref", PREF_APP_UPDATE_IDLETIME, 60), 600);
      if (idleService.idleTime / 1000 >= IDLE_TIME) {
        this._showUI(parent, uri, features, name, page, update);
        return;
      }
    }

    observer.service = Services.obs;
    observer.service.addObserver(observer, "quit-application");

    // bug 534090 - show the UI when idle for update available notifications.
    if (page == "updatesavailable") {
      this._showUIWhenIdle(parent, uri, features, name, page, update);
      return;
    }

    // Give the user x seconds to react before prompting as defined by
    // promptWaitTime
    observer.timer = Cc["@mozilla.org/timer;1"].
                     createInstance(Ci.nsITimer);
    observer.timer.initWithCallback(observer, update.promptWaitTime * 1000,
                                    observer.timer.TYPE_ONE_SHOT);
  },

  /**
   * Show the UI when the user was idle
   * @param   parent
   *          A parent window, can be null
   * @param   uri
   *          The URI string of the dialog to show
   * @param   name
   *          The Window Name of the dialog to show, in case it is already open
   *          and can merely be focused
   * @param   page
   *          The page of the wizard to be displayed, if one is already open.
   * @param   update
   *          An update to pass to the UI in the window arguments.
   *          Can be null
   */
  _showUIWhenIdle: function UP__showUIWhenIdle(parent, uri, features, name,
                                               page, update) {
    var idleService = Cc["@mozilla.org/widget/idleservice;1"].
                      getService(Ci.nsIIdleService);

    // Don't allow the preference to set a value greater than 600 seconds for the idle time.
    const IDLE_TIME = Math.min(getPref("getIntPref", PREF_APP_UPDATE_IDLETIME, 60), 600);
    if (idleService.idleTime / 1000 >= IDLE_TIME) {
      this._showUI(parent, uri, features, name, page, update);
    } else {
      var observer = {
        updatePrompt: this,
        observe(aSubject, aTopic, aData) {
          switch (aTopic) {
            case "idle":
              // If the update window is already open skip showing the UI
              if (!this.updatePrompt._getUpdateWindow())
                this.updatePrompt._showUI(parent, uri, features, name, page, update);
              // fall thru
            case "quit-application":
              idleService.removeIdleObserver(this, IDLE_TIME);
              Services.obs.removeObserver(this, "quit-application");
              break;
          }
        }
      };
      idleService.addIdleObserver(observer, IDLE_TIME);
      Services.obs.addObserver(observer, "quit-application");
    }
  },

  /**
   * Show the Update Checking UI
   * @param   parent
   *          A parent window, can be null
   * @param   uri
   *          The URI string of the dialog to show
   * @param   name
   *          The Window Name of the dialog to show, in case it is already open
   *          and can merely be focused
   * @param   page
   *          The page of the wizard to be displayed, if one is already open.
   * @param   update
   *          An update to pass to the UI in the window arguments.
   *          Can be null
   */
  _showUI: function UP__showUI(parent, uri, features, name, page, update) {
    var ary = null;
    if (update) {
      ary = Cc["@mozilla.org/array;1"].
            createInstance(Ci.nsIMutableArray);
      ary.appendElement(update);
    }

    var win = this._getUpdateWindow();
    if (win) {
      if (page && "setCurrentPage" in win)
        win.setCurrentPage(page);
      win.focus();
    } else {
      var openFeatures = "chrome,centerscreen,dialog=no,resizable=no,titlebar,toolbar=no";
      if (features)
        openFeatures += "," + features;
      Services.ww.openWindow(parent, uri, "", openFeatures, ary);
    }
  },

  classDescription: "Update Prompt",
  contractID: "@mozilla.org/updates/update-prompt;1",
  classID: Components.ID("{27ABA825-35B5-4018-9FDD-F99250A0E722}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdatePrompt])
};

var components = [UpdateService, Checker, UpdatePrompt, UpdateManager];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
