/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { AUSTLMY } = ChromeUtils.import(
  "resource://gre/modules/UpdateTelemetry.jsm"
);
const {
  Bits,
  BitsRequest,
  BitsUnknownError,
  BitsVerificationError,
} = ChromeUtils.import("resource://gre/modules/Bits.jsm");
const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser", "XMLHttpRequest"]);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  CertUtils: "resource://gre/modules/CertUtils.jsm",
  ctypes: "resource://gre/modules/ctypes.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  UpdateUtils: "resource://gre/modules/UpdateUtils.jsm",
  WindowsRegistry: "resource://gre/modules/WindowsRegistry.jsm",
});

if (AppConstants.ENABLE_REMOTE_AGENT) {
  XPCOMUtils.defineLazyServiceGetter(
    this,
    "RemoteAgent",
    "@mozilla.org/remote/agent;1",
    "nsIRemoteAgent"
  );
} else {
  this.RemoteAgent = { listening: false };
}

const UPDATESERVICE_CID = Components.ID(
  "{B3C290A6-3943-4B89-8BBE-C01EB7B3B311}"
);

const PREF_APP_UPDATE_ALTUPDATEDIRPATH = "app.update.altUpdateDirPath";
const PREF_APP_UPDATE_BACKGROUNDERRORS = "app.update.backgroundErrors";
const PREF_APP_UPDATE_BACKGROUNDMAXERRORS = "app.update.backgroundMaxErrors";
const PREF_APP_UPDATE_BITS_ENABLED = "app.update.BITS.enabled";
const PREF_APP_UPDATE_CANCELATIONS = "app.update.cancelations";
const PREF_APP_UPDATE_CANCELATIONS_OSX = "app.update.cancelations.osx";
const PREF_APP_UPDATE_CANCELATIONS_OSX_MAX = "app.update.cancelations.osx.max";
const PREF_APP_UPDATE_CHECK_ONLY_INSTANCE_ENABLED =
  "app.update.checkOnlyInstance.enabled";
const PREF_APP_UPDATE_CHECK_ONLY_INSTANCE_INTERVAL =
  "app.update.checkOnlyInstance.interval";
const PREF_APP_UPDATE_CHECK_ONLY_INSTANCE_TIMEOUT =
  "app.update.checkOnlyInstance.timeout";
const PREF_APP_UPDATE_DISABLEDFORTESTING = "app.update.disabledForTesting";
const PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS = "app.update.download.attempts";
const PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS = "app.update.download.maxAttempts";
const PREF_APP_UPDATE_ELEVATE_NEVER = "app.update.elevate.never";
const PREF_APP_UPDATE_ELEVATE_VERSION = "app.update.elevate.version";
const PREF_APP_UPDATE_ELEVATE_ATTEMPTS = "app.update.elevate.attempts";
const PREF_APP_UPDATE_ELEVATE_MAXATTEMPTS = "app.update.elevate.maxAttempts";
const PREF_APP_UPDATE_LANGPACK_ENABLED = "app.update.langpack.enabled";
const PREF_APP_UPDATE_LANGPACK_TIMEOUT = "app.update.langpack.timeout";
const PREF_APP_UPDATE_LOG = "app.update.log";
const PREF_APP_UPDATE_LOG_FILE = "app.update.log.file";
const PREF_APP_UPDATE_NOTIFYDURINGDOWNLOAD = "app.update.notifyDuringDownload";
const PREF_APP_UPDATE_PROMPTWAITTIME = "app.update.promptWaitTime";
const PREF_APP_UPDATE_SERVICE_ENABLED = "app.update.service.enabled";
const PREF_APP_UPDATE_SERVICE_ERRORS = "app.update.service.errors";
const PREF_APP_UPDATE_SERVICE_MAXERRORS = "app.update.service.maxErrors";
const PREF_APP_UPDATE_SOCKET_MAXERRORS = "app.update.socket.maxErrors";
const PREF_APP_UPDATE_SOCKET_RETRYTIMEOUT = "app.update.socket.retryTimeout";
const PREF_APP_UPDATE_STAGING_ENABLED = "app.update.staging.enabled";
const PREF_APP_UPDATE_URL_DETAILS = "app.update.url.details";
const PREF_NETWORK_PROXY_TYPE = "network.proxy.type";

const URI_BRAND_PROPERTIES = "chrome://branding/locale/brand.properties";
const URI_UPDATE_NS = "http://www.mozilla.org/2005/app-update";
const URI_UPDATES_PROPERTIES =
  "chrome://mozapps/locale/update/updates.properties";

const KEY_EXECUTABLE = "XREExeF";
const KEY_PROFILE_DIR = "ProfD";
const KEY_UPDROOT = "UpdRootD";

const DIR_UPDATES = "updates";

const FILE_ACTIVE_UPDATE_XML = "active-update.xml";
const FILE_BACKUP_UPDATE_LOG = "backup-update.log";
const FILE_BT_RESULT = "bt.result";
const FILE_LAST_UPDATE_LOG = "last-update.log";
const FILE_UPDATES_XML = "updates.xml";
const FILE_UPDATE_LOG = "update.log";
const FILE_UPDATE_MAR = "update.mar";
const FILE_UPDATE_STATUS = "update.status";
const FILE_UPDATE_TEST = "update.test";
const FILE_UPDATE_VERSION = "update.version";
const FILE_UPDATE_MESSAGES = "update_messages.log";

const STATE_NONE = "null";
const STATE_DOWNLOADING = "downloading";
const STATE_PENDING = "pending";
const STATE_PENDING_SERVICE = "pending-service";
const STATE_PENDING_ELEVATE = "pending-elevate";
const STATE_APPLYING = "applying";
const STATE_APPLIED = "applied";
const STATE_APPLIED_SERVICE = "applied-service";
const STATE_SUCCEEDED = "succeeded";
const STATE_DOWNLOAD_FAILED = "download-failed";
const STATE_FAILED = "failed";

// BITS will keep retrying a download after transient errors, unless this much
// time has passed since there has been download progress.
// Similarly to ...POLL_RATE_MS below, we are much more aggressive when the user
// is watching the download progress.
const BITS_IDLE_NO_PROGRESS_TIMEOUT_SECS = 3600; // 1 hour
const BITS_ACTIVE_NO_PROGRESS_TIMEOUT_SECS = 5;

// These value control how frequently we get updates from the BITS client on
// the progress made downloading. The difference between the two is that the
// active interval is the one used when the user is watching. The idle interval
// is the one used when no one is watching.
const BITS_IDLE_POLL_RATE_MS = 1000;
const BITS_ACTIVE_POLL_RATE_MS = 200;

// The values below used by this code are from common/updatererrors.h
const WRITE_ERROR = 7;
const ELEVATION_CANCELED = 9;
const SERVICE_UPDATER_COULD_NOT_BE_STARTED = 24;
const SERVICE_NOT_ENOUGH_COMMAND_LINE_ARGS = 25;
const SERVICE_UPDATER_SIGN_ERROR = 26;
const SERVICE_UPDATER_COMPARE_ERROR = 27;
const SERVICE_UPDATER_IDENTITY_ERROR = 28;
const SERVICE_STILL_APPLYING_ON_SUCCESS = 29;
const SERVICE_STILL_APPLYING_ON_FAILURE = 30;
const SERVICE_UPDATER_NOT_FIXED_DRIVE = 31;
const SERVICE_COULD_NOT_LOCK_UPDATER = 32;
const SERVICE_INSTALLDIR_ERROR = 33;
const WRITE_ERROR_ACCESS_DENIED = 35;
const WRITE_ERROR_CALLBACK_APP = 37;
const UNEXPECTED_STAGING_ERROR = 43;
const DELETE_ERROR_STAGING_LOCK_FILE = 44;
const SERVICE_COULD_NOT_COPY_UPDATER = 49;
const SERVICE_STILL_APPLYING_TERMINATED = 50;
const SERVICE_STILL_APPLYING_NO_EXIT_CODE = 51;
const SERVICE_COULD_NOT_IMPERSONATE = 58;
const WRITE_ERROR_FILE_COPY = 61;
const WRITE_ERROR_DELETE_FILE = 62;
const WRITE_ERROR_OPEN_PATCH_FILE = 63;
const WRITE_ERROR_PATCH_FILE = 64;
const WRITE_ERROR_APPLY_DIR_PATH = 65;
const WRITE_ERROR_CALLBACK_PATH = 66;
const WRITE_ERROR_FILE_ACCESS_DENIED = 67;
const WRITE_ERROR_DIR_ACCESS_DENIED = 68;
const WRITE_ERROR_DELETE_BACKUP = 69;
const WRITE_ERROR_EXTRACT = 70;

// Array of write errors to simplify checks for write errors
const WRITE_ERRORS = [
  WRITE_ERROR,
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
  WRITE_ERROR_EXTRACT,
];

// Array of write errors to simplify checks for service errors
const SERVICE_ERRORS = [
  SERVICE_UPDATER_COULD_NOT_BE_STARTED,
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
  SERVICE_STILL_APPLYING_NO_EXIT_CODE,
  SERVICE_COULD_NOT_IMPERSONATE,
];

// Error codes 80 through 99 are reserved for nsUpdateService.js and are not
// defined in common/updatererrors.h
const ERR_OLDER_VERSION_OR_SAME_BUILD = 90;
const ERR_UPDATE_STATE_NONE = 91;
const ERR_CHANNEL_CHANGE = 92;
const INVALID_UPDATER_STATE_CODE = 98;
const INVALID_UPDATER_STATUS_CODE = 99;

// Custom update error codes
const BACKGROUNDCHECK_MULTIPLE_FAILURES = 110;
const NETWORK_ERROR_OFFLINE = 111;

// Error codes should be < 1000. Errors above 1000 represent http status codes
const HTTP_ERROR_OFFSET = 1000;

// The is an HRESULT error that may be returned from the BITS interface
// indicating that access was denied. Often, this error code is returned when
// attempting to access a job created by a different user.
const HRESULT_E_ACCESSDENIED = -2147024891;

const DOWNLOAD_CHUNK_SIZE = 300000; // bytes

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
};

// The interval for the update xml write deferred task.
const XML_SAVER_INTERVAL_MS = 200;

// How long after a patch has downloaded should we wait for language packs to
// update before proceeding anyway.
const LANGPACK_UPDATE_DEFAULT_TIMEOUT = 300000;

// Interval between rechecks for other instances after the initial check finds
// at least one other instance.
const ONLY_INSTANCE_CHECK_DEFAULT_POLL_INTERVAL_MS = 5 * 60 * 1000; // 5 minutes

// Wait this long after detecting that another instance is running (having been
// polling that entire time) before giving up and applying the update anyway.
const ONLY_INSTANCE_CHECK_DEFAULT_TIMEOUT_MS = 6 * 60 * 60 * 1000; // 6 hours

// The other instance check timeout can be overridden via a pref, but we limit
// that value to this so that the pref can't effectively disable the feature.
const ONLY_INSTANCE_CHECK_MAX_TIMEOUT_MS = 2 * 24 * 60 * 60 * 1000; // 2 days

// Object to keep track of the current phase of the update and whether there
// has been a write failure for the phase so only one telemetry ping is made
// for the phase.
var gUpdateFileWriteInfo = { phase: null, failure: false };
var gUpdateMutexHandle = null;
// The permissions of the update directory should be fixed no more than once per
// session
var gUpdateDirPermissionFixAttempted = false;
// This is the file stream used for the log file.
var gLogfileOutputStream;
// This value will be set to true if it appears that BITS is being used by
// another user to download updates. We don't really want two users using BITS
// at once. Computers with many users (ex: a school computer), should not end
// up with dozens of BITS jobs.
var gBITSInUseByAnotherUser = false;
// Tracks whether an update is currently being staged. This is slightly more
// accurate than checking for STATE_APPLYING because there are brief periods of
// time at the beginning and end of staging when that will not be the state.
let gStagingInProgress = false;

XPCOMUtils.defineLazyGetter(this, "gLogEnabled", function aus_gLogEnabled() {
  return (
    Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG, false) ||
    Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false)
  );
});

XPCOMUtils.defineLazyGetter(
  this,
  "gLogfileEnabled",
  function aus_gLogfileEnabled() {
    return Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false);
  }
);

XPCOMUtils.defineLazyGetter(
  this,
  "gUpdateBundle",
  function aus_gUpdateBundle() {
    return Services.strings.createBundle(URI_UPDATES_PROPERTIES);
  }
);

/**
 * When a plain JS object is passed through xpconnect the other side sees a
 * wrapped version of the object instead of the real object. Since these two
 * objects are different they act as different keys for Map and WeakMap. However
 * xpconnect gives us a way to get the underlying JS object from the wrapper so
 * this function returns the JS object regardless of whether passed the JS
 * object or its wrapper for use in places where it is unclear which one you
 * have.
 */
function unwrap(obj) {
  return obj.wrappedJSObject ?? obj;
}

/**
 * When an update starts to download (and if the feature is enabled) the add-ons
 * manager starts downloading updated language packs for the new application
 * version. A promise is used to track whether those updates are complete so the
 * front-end is only notified that an application update is ready once the
 * language pack updates have been staged.
 *
 * In order to be able to access that promise from various places in the update
 * service they are cached in this map using the nsIUpdate object as a weak
 * owner. Note that the key should always be the result of calling the above
 * unwrap function on the nsIUpdate to ensure a consistent object is used as the
 * key.
 *
 * When the language packs finish staging the nsIUpdate entriy is removed from
 * this map so if the entry is still there then language pack updates are in
 * progress.
 */
const LangPackUpdates = new WeakMap();

/**
 * When we're polling to see if other running instances of the application have
 * exited, there's no need to ever start polling again in parallel. To prevent
 * doing that, we keep track of the promise that resolves when polling completes
 * and return that if a second simultaneous poll is requested, so that the
 * multiple callers end up waiting for the same promise to resolve.
 */
let gOtherInstancePollPromise;

/**
 * Query the update sync manager to see if another instance of this same
 * installation of this application is currently running, under the context of
 * any operating system user (not just the current one).
 * This function immediately returns the current, instantaneous status of any
 * other instances.
 *
 * @return true if at least one other instance is running, false if not
 */
function isOtherInstanceRunning(callback) {
  const checkEnabled = Services.prefs.getBoolPref(
    PREF_APP_UPDATE_CHECK_ONLY_INSTANCE_ENABLED,
    true
  );
  if (!checkEnabled) {
    LOG("isOtherInstanceRunning - disabled by pref, skipping check");
    return false;
  }

  try {
    let syncManager = Cc[
      "@mozilla.org/updates/update-sync-manager;1"
    ].getService(Ci.nsIUpdateSyncManager);
    return syncManager.isOtherInstanceRunning();
  } catch (ex) {
    LOG(`isOtherInstanceRunning - sync manager failed with exception: ${ex}`);
    return false;
  }
}

/**
 * Query the update sync manager to see if another instance of this same
 * installation of this application is currently running, under the context of
 * any operating system user (not just the one running this instance).
 * This function polls for the status of other instances continually
 * (asynchronously) until either none exist or a timeout expires.
 *
 * @return a Promise that resolves with false if at any point during polling no
 *         other instances can be found, or resolves with true if the timeout
 *         expires when other instances are still running
 */
function waitForOtherInstances() {
  // If we're already in the middle of a poll, reuse it rather than start again.
  if (gOtherInstancePollPromise) {
    return gOtherInstancePollPromise;
  }

  let timeout = Services.prefs.getIntPref(
    PREF_APP_UPDATE_CHECK_ONLY_INSTANCE_TIMEOUT,
    ONLY_INSTANCE_CHECK_DEFAULT_TIMEOUT_MS
  );
  // Don't allow the pref to set a super high timeout and break this feature.
  if (timeout > ONLY_INSTANCE_CHECK_MAX_TIMEOUT_MS) {
    timeout = ONLY_INSTANCE_CHECK_MAX_TIMEOUT_MS;
  }

  let interval = Services.prefs.getIntPref(
    PREF_APP_UPDATE_CHECK_ONLY_INSTANCE_INTERVAL,
    ONLY_INSTANCE_CHECK_DEFAULT_POLL_INTERVAL_MS
  );
  // Don't allow an interval longer than the timeout.
  interval = Math.min(interval, timeout);

  let iterations = 0;
  const maxIterations = Math.ceil(timeout / interval);

  gOtherInstancePollPromise = new Promise(function(resolve, reject) {
    let poll = function() {
      iterations++;
      if (!isOtherInstanceRunning()) {
        LOG("waitForOtherInstances - no other instances found, exiting");
        resolve(false);
        gOtherInstancePollPromise = undefined;
      } else if (iterations >= maxIterations) {
        LOG(
          "waitForOtherInstances - timeout expired while other instances " +
            "are still running"
        );
        resolve(true);
        gOtherInstancePollPromise = undefined;
      } else if (iterations + 1 == maxIterations && timeout % interval != 0) {
        // In case timeout isn't a multiple of interval, set the next timeout
        // for the remainder of the time rather than for the usual interval.
        setTimeout(poll, timeout % interval);
      } else {
        setTimeout(poll, interval);
      }
    };

    LOG("waitForOtherInstances - beginning polling");
    poll();
  });

  return gOtherInstancePollPromise;
}

/**
 * Tests to make sure that we can write to a given directory.
 *
 * @param updateTestFile a test file in the directory that needs to be tested.
 * @param createDirectory whether a test directory should be created.
 * @throws if we don't have right access to the directory.
 */
function testWriteAccess(updateTestFile, createDirectory) {
  const NORMAL_FILE_TYPE = Ci.nsIFile.NORMAL_FILE_TYPE;
  const DIRECTORY_TYPE = Ci.nsIFile.DIRECTORY_TYPE;
  if (updateTestFile.exists()) {
    updateTestFile.remove(false);
  }
  updateTestFile.create(
    createDirectory ? DIRECTORY_TYPE : NORMAL_FILE_TYPE,
    createDirectory ? FileUtils.PERMS_DIRECTORY : FileUtils.PERMS_FILE
  );
  updateTestFile.remove(false);
}

/**
 * Windows only function that closes a Win32 handle.
 *
 * @param handle The handle to close
 */
function closeHandle(handle) {
  if (handle) {
    let lib = ctypes.open("kernel32.dll");
    let CloseHandle = lib.declare(
      "CloseHandle",
      ctypes.winapi_abi,
      ctypes.int32_t /* success */,
      ctypes.void_t.ptr
    ); /* handle */
    CloseHandle(handle);
    lib.close();
  }
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
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  const INITIAL_OWN = 1;
  const ERROR_ALREADY_EXISTS = 0xb7;
  let lib = ctypes.open("kernel32.dll");
  let CreateMutexW = lib.declare(
    "CreateMutexW",
    ctypes.winapi_abi,
    ctypes.void_t.ptr /* return handle */,
    ctypes.void_t.ptr /* security attributes */,
    ctypes.int32_t /* initial owner */,
    ctypes.char16_t.ptr
  ); /* name */

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
 * @param aGlobal
 *        true if the function should return a global mutex. A global mutex is
 *        valid across different sessions.
 * @return Global mutex path
 */
function getPerInstallationMutexName(aGlobal = true) {
  if (AppConstants.platform != "win") {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  }

  let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
    Ci.nsICryptoHash
  );
  hasher.init(hasher.SHA1);

  let exeFile = Services.dirsvc.get(KEY_EXECUTABLE, Ci.nsIFile);

  let converter = Cc[
    "@mozilla.org/intl/scriptableunicodeconverter"
  ].createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(exeFile.path.toLowerCase());

  hasher.update(data, data.length);
  return (
    (aGlobal ? "Global\\" : "") + "MozillaUpdateMutex-" + hasher.finish(true)
  );
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
    let item = items.nextFile;
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
    LOG(
      "getElevationRequired - recursively testing write access on " +
        getInstallDirRoot().path
    );
    if (
      !getInstallDirRoot().isWritable() ||
      !areDirectoryEntriesWriteable(getInstallDirRoot())
    ) {
      LOG(
        "getElevationRequired - unable to write to application bundle, " +
          "elevation required"
      );
      return true;
    }
  } catch (ex) {
    LOG(
      "getElevationRequired - unable to write to application bundle, " +
        "elevation required. Exception: " +
        ex
    );
    return true;
  }
  LOG(
    "getElevationRequired - able to write to application bundle, elevation " +
      "not required"
  );
  return false;
}

/**
 * A promise that resolves when language packs are downloading or if no language
 * packs are being downloaded.
 */
function promiseLangPacksUpdated(update) {
  let promise = LangPackUpdates.get(unwrap(update));
  if (promise) {
    LOG(
      "promiseLangPacksUpdated - waiting for language pack updates to stage."
    );
    return promise;
  }

  // In case callers rely on a promise just return an already resolved promise.
  return Promise.resolve();
}

/**
 * Determines whether or not an update can be applied. This is always true on
 * Windows when the service is used. On Mac OS X and Linux, if the user has
 * write access to the update directory this will return true because on OSX we
 * offer users the option to perform an elevated update when necessary and on
 * Linux the update directory is located in the application directory.
 *
 * @return true if an update can be applied, false otherwise
 */
function getCanApplyUpdates() {
  try {
    // Check if it is possible to write to the update directory so clients won't
    // repeatedly try to apply an update without the ability to complete the
    // update process which requires write access to the update directory.
    let updateTestFile = getUpdateFile([FILE_UPDATE_TEST]);
    LOG("getCanApplyUpdates - testing write access " + updateTestFile.path);
    testWriteAccess(updateTestFile, false);
  } catch (e) {
    LOG(
      "getCanApplyUpdates - unable to apply updates without write " +
        "access to the update directory. Exception: " +
        e
    );
    // Attempt to fix the update directory permissions. If successful the next
    // time this function is called the write access check to the update
    // directory will succeed.
    fixUpdateDirectoryPermissions();
    return false;
  }

  if (AppConstants.platform == "macosx") {
    LOG(
      "getCanApplyUpdates - bypass the write since elevation can be used " +
        "on Mac OS X"
    );
    return true;
  }

  if (shouldUseService()) {
    LOG(
      "getCanApplyUpdates - bypass the write checks because the Windows " +
        "Maintenance Service can be used"
    );
    return true;
  }

  try {
    if (AppConstants.platform == "win") {
      // On Windows when the maintenance service isn't used updates can still be
      // performed in a location requiring admin privileges by the client
      // accepting a UAC prompt from an elevation request made by the updater.
      // Whether the client can elevate (e.g. has a split token) is determined
      // in nsXULAppInfo::GetUserCanElevate which is located in nsAppRunner.cpp.
      let userCanElevate = Services.appinfo.QueryInterface(Ci.nsIWinAppHelper)
        .userCanElevate;
      if (!userCanElevate) {
        // if we're unable to create the test file this will throw an exception.
        let appDirTestFile = getAppBaseDir();
        appDirTestFile.append(FILE_UPDATE_TEST);
        LOG("getCanApplyUpdates - testing write access " + appDirTestFile.path);
        if (appDirTestFile.exists()) {
          appDirTestFile.remove(false);
        }
        appDirTestFile.create(
          Ci.nsIFile.NORMAL_FILE_TYPE,
          FileUtils.PERMS_FILE
        );
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
XPCOMUtils.defineLazyGetter(
  this,
  "gCanStageUpdatesSession",
  function aus_gCSUS() {
    if (getElevationRequired()) {
      LOG(
        "gCanStageUpdatesSession - unable to stage updates because elevation " +
          "is required."
      );
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
      LOG(
        "gCanStageUpdatesSession - testing write access " + updateTestFile.path
      );
      testWriteAccess(updateTestFile, true);
      if (AppConstants.platform != "macosx") {
        // On all platforms except Mac, we need to test the parent directory as
        // well, as we need to be able to move files in that directory during the
        // replacing step.
        updateTestFile = getInstallDirRoot().parent;
        updateTestFile.append(FILE_UPDATE_TEST);
        LOG(
          "gCanStageUpdatesSession - testing write access " +
            updateTestFile.path
        );
        updateTestFile.createUnique(
          Ci.nsIFile.DIRECTORY_TYPE,
          FileUtils.PERMS_DIRECTORY
        );
        updateTestFile.remove(false);
      }
    } catch (e) {
      LOG("gCanStageUpdatesSession - unable to stage updates. Exception: " + e);
      // No write privileges
      return false;
    }

    LOG("gCanStageUpdatesSession - able to stage updates");
    return true;
  }
);

/**
 * Whether or not the application can stage an update.
 *
 * @return true if updates can be staged.
 */
function getCanStageUpdates() {
  // If staging updates are disabled, then just bail out!
  if (!Services.prefs.getBoolPref(PREF_APP_UPDATE_STAGING_ENABLED, false)) {
    LOG(
      "getCanStageUpdates - staging updates is disabled by preference " +
        PREF_APP_UPDATE_STAGING_ENABLED
    );
    return false;
  }

  if (AppConstants.platform == "win" && shouldUseService()) {
    // No need to perform directory write checks, the maintenance service will
    // be able to write to all directories.
    LOG("getCanStageUpdates - able to stage updates using the service");
    return true;
  }

  if (!hasUpdateMutex()) {
    LOG(
      "getCanStageUpdates - unable to apply updates because another " +
        "instance of the application is already handling updates for this " +
        "installation."
    );
    return false;
  }

  return gCanStageUpdatesSession;
}

/*
 * Whether or not the application can use BITS to download updates.
 *
 * @return A string with one of these values:
 *           CanUseBits
 *           NoBits_NotWindows
 *           NoBits_FeatureOff
 *           NoBits_Pref
 *           NoBits_Proxy
 *           NoBits_OtherUser
 *         These strings are directly compatible with the categories for
 *         UPDATE_CAN_USE_BITS_EXTERNAL and UPDATE_CAN_USE_BITS_NOTIFY telemetry
 *         probes. If this function is made to return other values, they should
 *         also be added to the labels lists for those probes in Histograms.json
 */
function getCanUseBits() {
  if (AppConstants.platform != "win") {
    LOG("getCanUseBits - Not using BITS because this is not Windows");
    return "NoBits_NotWindows";
  }
  if (!AppConstants.MOZ_BITS_DOWNLOAD) {
    LOG("getCanUseBits - Not using BITS because the feature is disabled");
    return "NoBits_FeatureOff";
  }

  if (!Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED, true)) {
    LOG("getCanUseBits - Not using BITS. Disabled by pref.");
    return "NoBits_Pref";
  }
  if (gBITSInUseByAnotherUser) {
    LOG("getCanUseBits - Not using BITS. Already in use by another user");
    return "NoBits_OtherUser";
  }
  // Firefox support for passing proxies to BITS is still rudimentary.
  // For now, disable BITS support on configurations that are not using the
  // standard system proxy.
  let defaultProxy = Ci.nsIProtocolProxyService.PROXYCONFIG_SYSTEM;
  if (
    Services.prefs.getIntPref(PREF_NETWORK_PROXY_TYPE, defaultProxy) !=
      defaultProxy &&
    !Cu.isInAutomation
  ) {
    LOG("getCanUseBits - Not using BITS because of proxy usage");
    return "NoBits_Proxy";
  }
  LOG("getCanUseBits - BITS can be used to download updates");
  return "CanUseBits";
}

/**
 * Logs a string to the error console. If enabled, also logs to the update
 * messages file.
 * @param   string
 *          The string to write to the error console.
 */
function LOG(string) {
  if (gLogEnabled) {
    dump("*** AUS:SVC " + string + "\n");
    if (!Cu.isInAutomation) {
      Services.console.logStringMessage("AUS:SVC " + string);
    }

    if (gLogfileEnabled) {
      if (!gLogfileOutputStream) {
        let logfile = Services.dirsvc.get(KEY_PROFILE_DIR, Ci.nsIFile);
        logfile.append(FILE_UPDATE_MESSAGES);
        gLogfileOutputStream = FileUtils.openAtomicFileOutputStream(logfile);
      }

      try {
        let encoded = new TextEncoder().encode(string + "\n");
        gLogfileOutputStream.write(encoded, encoded.length);
        gLogfileOutputStream.flush();
      } catch (e) {
        dump("*** AUS:SVC Unable to write to messages file: " + e + "\n");
        Services.console.logStringMessage(
          "AUS:SVC Unable to write to messages file: " + e
        );
      }
    }
  }
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
  if (Cu.isInAutomation) {
    // This allows tests to use an alternate updates directory so they can test
    // startup behavior.
    const MAGIC_TEST_ROOT_PREFIX = "<test-root>";
    const PREF_TEST_ROOT = "mochitest.testRoot";
    let alternatePath = Services.prefs.getCharPref(
      PREF_APP_UPDATE_ALTUPDATEDIRPATH,
      null
    );
    if (alternatePath && alternatePath.startsWith(MAGIC_TEST_ROOT_PREFIX)) {
      let testRoot = Services.prefs.getCharPref(PREF_TEST_ROOT);
      let relativePath = alternatePath.substring(MAGIC_TEST_ROOT_PREFIX.length);
      if (AppConstants.platform == "win") {
        relativePath = relativePath.replace(/\//g, "\\");
      }
      alternatePath = testRoot + relativePath;
      let updateDir = Cc["@mozilla.org/file/local;1"].createInstance(
        Ci.nsIFile
      );
      updateDir.initWithPath(alternatePath);
      for (let i = 0; i < pathArray.length; ++i) {
        updateDir.append(pathArray[i]);
      }
      return updateDir;
    }
  }

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
    LOG(
      "getStatusTextFromCode - transfer error: " + reason + ", code: " + code
    );
  } catch (e) {
    // Use the default reason
    reason = gUpdateBundle.GetStringFromName("check_error-" + defaultCode);
    LOG(
      "getStatusTextFromCode - transfer error: " +
        reason +
        ", default code: " +
        defaultCode
    );
  }
  return reason;
}

/**
 * Get the Ready Update directory. This is the directory that an update
 * should reside in after download has completed but before it has been
 * installed and cleaned up.
 * @return The ready updates directory, as a nsIFile object
 */
function getReadyUpdateDir() {
  return getUpdateDirCreate([DIR_UPDATES, "0"]);
}

/**
 * Get the Downloading Update directory. This is the directory that an update
 * should reside in during download. Once download is completed, it will be
 * moved to the Ready Update directory.
 * @return The downloading update directory, as a nsIFile object
 */
function getDownloadingUpdateDir() {
  return getUpdateDirCreate([DIR_UPDATES, "downloading"]);
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
 * Reads the binary transparency result file from the given directory.
 * Removes the file if it is present (so don't call this twice and expect a
 * result the second time).
 * @param   dir
 *          The dir to look for an update.bt file in
 * @return  A error code from verifying binary transparency information or null
 *          if the file was not present (indicating there was no error).
 */
function readBinaryTransparencyResult(dir) {
  let binaryTransparencyResultFile = dir.clone();
  binaryTransparencyResultFile.append(FILE_BT_RESULT);
  let result = readStringFromFile(binaryTransparencyResultFile);
  LOG(
    "readBinaryTransparencyResult - result: " +
      result +
      ", path: " +
      binaryTransparencyResultFile.path
  );
  // If result is non-null, the file exists. We should remove it to avoid
  // double-reporting this result.
  if (result) {
    binaryTransparencyResultFile.remove(false);
  }
  return result;
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
  let success = writeStringToFile(statusFile, state);
  if (!success) {
    handleCriticalWriteFailure(statusFile.path);
  }
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
  let success = writeStringToFile(versionFile, version);
  if (!success) {
    handleCriticalWriteFailure(versionFile.path);
  }
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
  if (
    !AppConstants.MOZ_MAINTENANCE_SERVICE ||
    !isServiceInstalled() ||
    !Services.prefs.getBoolPref(PREF_APP_UPDATE_SERVICE_ENABLED, false)
  ) {
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
    let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    wrk.open(
      wrk.ROOT_KEY_LOCAL_MACHINE,
      "SOFTWARE\\Mozilla\\MaintenanceService",
      wrk.ACCESS_READ | wrk.WOW64_64
    );
    installed = wrk.readIntValue("Installed");
    wrk.close();
  } catch (e) {}
  installed = installed == 1; // convert to bool
  LOG("isServiceInstalled - returning " + installed);
  return installed;
}

/**
 * Gets the appropriate pending update state. Returns STATE_PENDING_SERVICE,
 * STATE_PENDING_ELEVATE, or STATE_PENDING.
 */
function getBestPendingState() {
  if (shouldUseService()) {
    return STATE_PENDING_SERVICE;
  } else if (getElevationRequired()) {
    return STATE_PENDING_ELEVATE;
  }
  return STATE_PENDING;
}

/**
 * Removes the contents of the ready update directory and rotates the update
 * logs when present. If the update.log exists in the patch directory this will
 * move the last-update.log if it exists to backup-update.log in the parent
 * directory of the patch directory and then move the update.log in the patch
 * directory to last-update.log in the parent directory of the patch directory.
 *
 * @param aRemovePatchFiles (optional, defaults to true)
 *        When true the update's patch directory contents are removed.
 */
function cleanUpReadyUpdateDir(aRemovePatchFiles = true) {
  let updateDir;
  try {
    updateDir = getReadyUpdateDir();
  } catch (e) {
    LOG(
      "cleanUpReadyUpdateDir - unable to get the updates patch directory. " +
        "Exception: " +
        e
    );
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
        LOG(
          "cleanUpReadyUpdateDir - failed to rename file " +
            logFile.path +
            " to " +
            FILE_BACKUP_UPDATE_LOG
        );
      }
    }

    try {
      updateLogFile.moveTo(dir, FILE_LAST_UPDATE_LOG);
    } catch (e) {
      LOG(
        "cleanUpReadyUpdateDir - failed to rename file " +
          updateLogFile.path +
          " to " +
          FILE_LAST_UPDATE_LOG
      );
    }
  }

  if (aRemovePatchFiles) {
    let dirEntries = updateDir.directoryEntries;
    while (dirEntries.hasMoreElements()) {
      let file = dirEntries.nextFile;
      // Now, recursively remove this file.  The recursive removal is needed for
      // Mac OSX because this directory will contain a copy of updater.app,
      // which is itself a directory and the MozUpdater directory on platforms
      // other than Windows.
      try {
        file.remove(true);
      } catch (e) {
        LOG("cleanUpReadyUpdateDir - failed to remove file " + file.path);
      }
    }
  }
}

/**
 * Removes the contents of the update download directory.
 *
 */
function cleanUpDownloadingUpdateDir() {
  let updateDir;
  try {
    updateDir = getDownloadingUpdateDir();
  } catch (e) {
    LOG(
      "cleanUpDownloadUpdatesDir - unable to get the updates patch " +
        "directory. Exception: " +
        e
    );
    return;
  }

  let dirEntries = updateDir.directoryEntries;
  while (dirEntries.hasMoreElements()) {
    let file = dirEntries.nextFile;
    // Now, recursively remove this file.
    try {
      file.remove(true);
    } catch (e) {
      LOG("cleanUpDownloadUpdatesDir - failed to remove file " + file.path);
    }
  }
}

/**
 * Clean up the updates list and the directory that contains the update that
 * is ready to be installed.
 *
 * Note - This function causes a state transition to either STATE_DOWNLOADING
 *        or STATE_NONE, depending on whether an update download is in progress.
 */
function cleanupReadyUpdate() {
  // Move the update from the Active Update list into the Past Updates list.
  var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
    Ci.nsIUpdateManager
  );
  if (um.readyUpdate) {
    um.addUpdateToHistory(um.readyUpdate);
    um.readyUpdate = null;
  }
  um.saveUpdates();

  let readyUpdateDir = getReadyUpdateDir();
  let shouldSetDownloadingStatus =
    um.downloadingUpdate || readStatusFile(readyUpdateDir) == STATE_DOWNLOADING;

  // Now trash the ready update directory, since we're done with it
  cleanUpReadyUpdateDir();

  // We need to handle two similar cases here.
  // The first is where we clean up the ready updates directory while we are in
  // the downloading state. In this case, we remove the update.status file that
  // says we are downloading, even though we should remain in that state.
  // The second case is when we clean up a ready update, but there is also a
  // downloading update (in which case the update status file's state will
  // reflect the state of the ready update, not the downloading one). In that
  // case, instead of reverting to STATE_NONE (which is what we do by removing
  // the status file), we should set our state to downloading.
  if (shouldSetDownloadingStatus) {
    writeStatusFile(readyUpdateDir, STATE_DOWNLOADING);
  }
}

/**
 * Clean up updates list and the directory that the currently downloading update
 * is downloaded to.
 *
 * Note - This function may cause a state transition. If the current state is
 *        STATE_DOWNLOADING, this will cause it to change to STATE_NONE.
 */
function cleanupDownloadingUpdate() {
  // Move the update from the Active Update list into the Past Updates list.
  var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
    Ci.nsIUpdateManager
  );
  if (um.downloadingUpdate) {
    um.addUpdateToHistory(um.downloadingUpdate);
    um.downloadingUpdate = null;
  }
  um.saveUpdates();

  // Now trash the update download directory, since we're done with it
  cleanUpDownloadingUpdateDir();

  // If the update status file says we are downloading, we should remove that
  // too, since we aren't doing that anymore.
  let readyUpdateDir = getReadyUpdateDir();
  let status = readStatusFile(readyUpdateDir);
  if (status == STATE_DOWNLOADING) {
    let statusFile = readyUpdateDir.clone();
    statusFile.append(FILE_UPDATE_STATUS);
    statusFile.remove();
  }
}

/**
 * Clean up updates list, the ready update directory, and the downloading update
 * directory.
 *
 * This is more efficient than calling
 *   cleanupReadyUpdate();
 *   cleanupDownloadingUpdate();
 * because those need some special handling of the update status file to make
 * sure that, for example, cleaning up a ready update doesn't make us forget
 * that we are downloading an update. When we cleanup both updates, we don't
 * need to worry about things like that.
 *
 * Note - This function causes a state transition to STATE_NONE.
 */
function cleanupActiveUpdates() {
  // Move the update from the Active Update list into the Past Updates list.
  var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
    Ci.nsIUpdateManager
  );
  if (um.readyUpdate) {
    um.addUpdateToHistory(um.readyUpdate);
    um.readyUpdate = null;
  }
  if (um.downloadingUpdate) {
    um.addUpdateToHistory(um.downloadingUpdate);
    um.downloadingUpdate = null;
  }
  um.saveUpdates();

  // Now trash both active update directories, since we're done with them
  cleanUpReadyUpdateDir();
  cleanUpDownloadingUpdateDir();
}

/**
 * Writes a string of text to a file.  A newline will be appended to the data
 * written to the file.  This function only works with ASCII text.
 * @param file An nsIFile indicating what file to write to.
 * @param text A string containing the text to write to the file.
 * @return true on success, false on failure.
 */
function writeStringToFile(file, text) {
  try {
    let fos = FileUtils.openSafeFileOutputStream(file);
    text += "\n";
    fos.write(text, text.length);
    FileUtils.closeSafeFileOutputStream(fos);
  } catch (e) {
    return false;
  }
  return true;
}

function readStringFromInputStream(inputStream) {
  var sis = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
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
  var fis = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  fis.init(file, FileUtils.MODE_RDONLY, FileUtils.PERMS_FILE, 0);
  return readStringFromInputStream(fis);
}

function handleUpdateFailure(update, errorCode) {
  update.errorCode = parseInt(errorCode);
  if (WRITE_ERRORS.includes(update.errorCode)) {
    writeStatusFile(getReadyUpdateDir(), (update.state = STATE_PENDING));
    return true;
  }

  if (update.errorCode == ELEVATION_CANCELED) {
    let elevationAttempts = Services.prefs.getIntPref(
      PREF_APP_UPDATE_ELEVATE_ATTEMPTS,
      0
    );
    elevationAttempts++;
    Services.prefs.setIntPref(
      PREF_APP_UPDATE_ELEVATE_ATTEMPTS,
      elevationAttempts
    );
    let maxAttempts = Math.min(
      Services.prefs.getIntPref(PREF_APP_UPDATE_ELEVATE_MAXATTEMPTS, 2),
      10
    );

    if (elevationAttempts > maxAttempts) {
      LOG(
        "handleUpdateFailure - notifying observers of error. " +
          "topic: update-error, status: elevation-attempts-exceeded"
      );
      Services.obs.notifyObservers(
        update,
        "update-error",
        "elevation-attempts-exceeded"
      );
    } else {
      LOG(
        "handleUpdateFailure - notifying observers of error. " +
          "topic: update-error, status: elevation-attempt-failed"
      );
      Services.obs.notifyObservers(
        update,
        "update-error",
        "elevation-attempt-failed"
      );
    }

    let cancelations = Services.prefs.getIntPref(
      PREF_APP_UPDATE_CANCELATIONS,
      0
    );
    cancelations++;
    Services.prefs.setIntPref(PREF_APP_UPDATE_CANCELATIONS, cancelations);
    if (AppConstants.platform == "macosx") {
      let osxCancelations = Services.prefs.getIntPref(
        PREF_APP_UPDATE_CANCELATIONS_OSX,
        0
      );
      osxCancelations++;
      Services.prefs.setIntPref(
        PREF_APP_UPDATE_CANCELATIONS_OSX,
        osxCancelations
      );
      let maxCancels = Services.prefs.getIntPref(
        PREF_APP_UPDATE_CANCELATIONS_OSX_MAX,
        DEFAULT_CANCELATIONS_OSX_MAX
      );
      // Prevent the preference from setting a value greater than 5.
      maxCancels = Math.min(maxCancels, 5);
      if (osxCancelations >= maxCancels) {
        cleanupReadyUpdate();
      } else {
        writeStatusFile(
          getReadyUpdateDir(),
          (update.state = STATE_PENDING_ELEVATE)
        );
      }
      update.statusText = gUpdateBundle.GetStringFromName("elevationFailure");
    } else {
      writeStatusFile(getReadyUpdateDir(), (update.state = STATE_PENDING));
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
    var failCount = Services.prefs.getIntPref(
      PREF_APP_UPDATE_SERVICE_ERRORS,
      0
    );
    var maxFail = Services.prefs.getIntPref(
      PREF_APP_UPDATE_SERVICE_MAXERRORS,
      DEFAULT_SERVICE_MAX_ERRORS
    );
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
      Services.prefs.setIntPref(PREF_APP_UPDATE_SERVICE_ERRORS, failCount);
    }

    writeStatusFile(getReadyUpdateDir(), (update.state = STATE_PENDING));
    return true;
  }

  if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_SERVICE_ERRORS)) {
    Services.prefs.clearUserPref(PREF_APP_UPDATE_SERVICE_ERRORS);
  }

  return false;
}

/**
 * Return the first UpdatePatch with the given type.
 * @param   update
 *          A nsIUpdate object to search through for a patch of the desired
 *          type.
 * @param   patch_type
 *          The type of the patch ("complete" or "partial")
 * @return  A nsIUpdatePatch object matching the type specified
 */
function getPatchOfType(update, patch_type) {
  for (var i = 0; i < update.patchCount; ++i) {
    var patch = update.getPatchAt(i);
    if (patch && patch.type == patch_type) {
      return patch;
    }
  }
  return null;
}

/**
 * Fall back to downloading a complete update in case an update has failed.
 *
 * @param postStaging true if we have just attempted to stage an update.
 */
function handleFallbackToCompleteUpdate(postStaging) {
  // If we failed to install an update, we need to fall back to a complete
  // update. If the install directory has been modified, more partial updates
  // will fail for the same reason. Since we only download partial updates
  // while there is already an update downloaded, we don't have to check the
  // downloading update, we can be confident that we are not downloading the
  // right thing at the moment.

  let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
    Ci.nsIUpdateManager
  );
  let aus = Cc["@mozilla.org/updates/update-service;1"].getService(
    Ci.nsIApplicationUpdateService
  );

  // The downloading update will be newer than the ready update, so use that
  // update, if it exists.
  let update = um.downloadingUpdate || um.readyUpdate;
  if (!update) {
    LOG(
      "handleFallbackToCompleteUpdate - Unable to find an update to fall " +
        "back to."
    );
    return;
  }

  aus.stopDownload();
  cleanupActiveUpdates();

  if (!update.selectedPatch) {
    // If we don't have a partial patch selected but a partial is available,
    // _selectPatch() will download that instead of the complete patch.
    let patch = getPatchOfType(update, "partial");
    if (patch) {
      patch.selected = true;
    }
  }

  update.statusText = gUpdateBundle.GetStringFromName("patchApplyFailure");
  var oldType = update.selectedPatch ? update.selectedPatch.type : "complete";
  if (update.selectedPatch && oldType == "partial" && update.patchCount == 2) {
    // Partial patch application failed, try downloading the complete
    // update in the background instead.
    LOG(
      "handleFallbackToCompleteUpdate - install of partial patch " +
        "failed, downloading complete patch"
    );
    var success = aus.downloadUpdate(update, !postStaging);
    if (!success) {
      cleanupDownloadingUpdate();
    }
  } else {
    LOG(
      "handleFallbackToCompleteUpdate - install of complete or " +
        "only one patch offered failed. Notifying observers. topic: " +
        "update-error, status: unknown, " +
        "update.patchCount: " +
        update.patchCount +
        ", " +
        "oldType: " +
        oldType
    );
    Services.obs.notifyObservers(update, "update-error", "unknown");
  }
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
  if (parts.length) {
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
      // Note: Do not use stateCode 14 here. It is defined in
      // UpdateTelemetry.jsm
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
  let binaryTransparencyResult = readBinaryTransparencyResult(
    getReadyUpdateDir()
  );
  if (binaryTransparencyResult) {
    AUSTLMY.pingBinaryTransparencyResult(
      suffix,
      parseInt(binaryTransparencyResult)
    );
  }
  AUSTLMY.pingStateCode(suffix, stateCode);
}

/**
 * Asynchronously fixes the update directory permissions. This is currently only
 * available on Windows.
 *
 * @return true if the permission-fixing process was started, and false if the
 *         permission-fixing process was not started or the platform is not
 *         supported.
 */
function fixUpdateDirectoryPermissions() {
  if (AppConstants.platform != "win") {
    LOG(
      "There is currently no implementation for fixing update directory " +
        "permissions on this platform"
    );
    return false;
  }

  if (!gUpdateDirPermissionFixAttempted) {
    // Never try to fix permissions more than one time during a session.
    gUpdateDirPermissionFixAttempted = true;

    LOG("Attempting to fix update directory permissions");
    try {
      Cc["@mozilla.org/updates/update-processor;1"]
        .createInstance(Ci.nsIUpdateProcessor)
        .fixUpdateDirectoryPerms(shouldUseService());
    } catch (e) {
      LOG(
        "Attempt to fix update directory permissions failed. Exception: " + e
      );
      return false;
    }
    return true;
  }
  return false;
}

/**
 * This function should be called whenever we fail to write to a file required
 * for update to function. This function will, if possible, attempt to fix the
 * file permissions. If the file permissions cannot be fixed, the user will be
 * prompted to reinstall.
 *
 * All functionality happens asynchronously.
 *
 * Returns false if the permission-fixing process cannot be started. Since this
 * is asynchronous, a true return value does not mean that the permissions were
 * actually fixed.
 *
 * @param path A string representing the path that could not be written. This
 *             value will only be used for logging purposes.
 */
function handleCriticalWriteFailure(path) {
  LOG(
    "handleCriticalWriteFailure - Unable to write to critical update file: " +
      path
  );
  if (!gUpdateFileWriteInfo.failure) {
    gUpdateFileWriteInfo.failure = true;
    let patchType = AUSTLMY.PATCH_UNKNOWN;
    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    let update = um.readyUpdate || um.downloadingUpdate;
    if (update) {
      let patch = update.selectedPatch;
      if (patch.type == "complete") {
        patchType = AUSTLMY.PATCH_COMPLETE;
      } else if (patch.type == "partial") {
        patchType = AUSTLMY.PATCH_PARTIAL;
      }
    }

    if (gUpdateFileWriteInfo.phase == "check") {
      let updateServiceInstance = UpdateServiceFactory.createInstance();
      let pingSuffix = updateServiceInstance._pingSuffix;
      if (!pingSuffix) {
        // If pingSuffix isn't defined then this this is a manual check which
        // isn't recorded at this time.
        AUSTLMY.pingCheckCode(pingSuffix, AUSTLMY.CHK_ERR_WRITE_FAILURE);
      }
    } else if (gUpdateFileWriteInfo.phase == "download") {
      AUSTLMY.pingDownloadCode(patchType, AUSTLMY.DWNLD_ERR_WRITE_FAILURE);
    } else if (gUpdateFileWriteInfo.phase == "stage") {
      let suffix = patchType + "_" + AUSTLMY.STAGE;
      AUSTLMY.pingStateCode(suffix, AUSTLMY.STATE_WRITE_FAILURE);
    } else if (gUpdateFileWriteInfo.phase == "startup") {
      let suffix = patchType + "_" + AUSTLMY.STARTUP;
      AUSTLMY.pingStateCode(suffix, AUSTLMY.STATE_WRITE_FAILURE);
    } else {
      // Temporary failure code to see if there are failures without an update
      // phase.
      AUSTLMY.pingDownloadCode(
        patchType,
        AUSTLMY.DWNLD_UNKNOWN_PHASE_ERR_WRITE_FAILURE
      );
    }
  }

  return fixUpdateDirectoryPermissions();
}

/**
 * This is a convenience function for calling the above function depending on a
 * boolean success value.
 *
 * @param wroteSuccessfully A boolean representing whether or not the write was
 *                          successful. When this is true, this function does
 *                          nothing.
 * @param path A string representing the path to the file that the operation
 *             attempted to write to. This value is only used for logging
 *             purposes.
 */
function handleCriticalWriteResult(wroteSuccessfully, path) {
  if (!wroteSuccessfully) {
    handleCriticalWriteFailure(path);
  }
}

/**
 * This returns true if the passed update is the same version or older than the
 * version and build ID values passed. Otherwise it returns false.
 */
function updateIsAtLeastAsOldAs(update, version, buildID) {
  if (!update || !update.appVersion || !update.buildID) {
    return false;
  }
  let versionComparison = Services.vc.compare(update.appVersion, version);
  return (
    versionComparison < 0 ||
    (versionComparison == 0 && update.buildID == buildID)
  );
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
  this.errorCode = 0;
  this.finalURL = null;
  this.state = STATE_NONE;

  for (let i = 0; i < patch.attributes.length; ++i) {
    var attr = patch.attributes.item(i);
    // If an undefined value is saved to the xml file it will be a string when
    // it is read from the xml file.
    if (attr.value == "undefined") {
      continue;
    }
    switch (attr.name) {
      case "xmlns":
        // Don't save the XML namespace.
        break;
      case "selected":
        this.selected = attr.value == "true";
        break;
      case "size":
        if (0 == parseInt(attr.value)) {
          LOG("UpdatePatch:init - 0-sized patch!");
          throw Components.Exception("", Cr.NS_ERROR_ILLEGAL_VALUE);
        }
        this[attr.name] = attr.value;
        break;
      case "errorCode":
        if (attr.value) {
          let val = parseInt(attr.value);
          // This will evaluate to false if the value is 0 but that's ok since
          // this.errorCode is set to the default of 0 above.
          if (val) {
            this.errorCode = val;
          }
        }
        break;
      case "finalURL":
      case "state":
      case "type":
      case "URL":
        this[attr.name] = attr.value;
        break;
      default:
        if (!this._attrNames.includes(attr.name)) {
          // Set nsIPropertyBag properties that were read from the xml file.
          this.setProperty(attr.name, attr.value);
        }
        break;
    }
  }
}
UpdatePatch.prototype = {
  // nsIUpdatePatch attribute names used to prevent nsIWritablePropertyBag from
  // over writing nsIUpdatePatch attributes.
  _attrNames: [
    "errorCode",
    "finalURL",
    "selected",
    "size",
    "state",
    "type",
    "URL",
  ],

  /**
   * See nsIUpdateService.idl
   */
  serialize: function UpdatePatch_serialize(updates) {
    var patch = updates.createElementNS(URI_UPDATE_NS, "patch");
    patch.setAttribute("size", this.size);
    patch.setAttribute("type", this.type);
    patch.setAttribute("URL", this.URL);
    // Don't write an errorCode if it evaluates to false since 0 is the same as
    // no error code.
    if (this.errorCode) {
      patch.setAttribute("errorCode", this.errorCode);
    }
    // finalURL is not available until after the download has started
    if (this.finalURL) {
      patch.setAttribute("finalURL", this.finalURL);
    }
    // The selected patch is the only patch that should have this attribute.
    if (this.selected) {
      patch.setAttribute("selected", this.selected);
    }
    if (this.state != STATE_NONE) {
      patch.setAttribute("state", this.state);
    }

    for (let [name, value] of Object.entries(this._properties)) {
      if (value.present && !this._attrNames.includes(name)) {
        patch.setAttribute(name, value.data);
      }
    }
    return patch;
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  setProperty: function UpdatePatch_setProperty(name, value) {
    if (this._attrNames.includes(name)) {
      throw Components.Exception(
        "Illegal value '" +
          name +
          "' (attribute exists on nsIUpdatePatch) " +
          "when calling method: [nsIWritablePropertyBag::setProperty]",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    this._properties[name] = { data: value, present: true };
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  deleteProperty: function UpdatePatch_deleteProperty(name) {
    if (this._attrNames.includes(name)) {
      throw Components.Exception(
        "Illegal value '" +
          name +
          "' (attribute exists on nsIUpdatePatch) " +
          "when calling method: [nsIWritablePropertyBag::deleteProperty]",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    if (name in this._properties) {
      this._properties[name].present = false;
    } else {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
  },

  /**
   * See nsIPropertyBag.idl
   *
   * Note: this only contains the nsIPropertyBag name / value pairs and not the
   *       nsIUpdatePatch name / value pairs.
   */
  get enumerator() {
    return this.enumerate();
  },

  *enumerate() {
    // An nsISupportsInterfacePointer is used so creating an array using
    // Array.from will retain the QueryInterface for nsIProperty.
    let ip = Cc["@mozilla.org/supports-interface-pointer;1"].createInstance(
      Ci.nsISupportsInterfacePointer
    );
    let qi = ChromeUtils.generateQI(["nsIProperty"]);
    for (let [name, value] of Object.entries(this._properties)) {
      if (value.present && !this._attrNames.includes(name)) {
        // The nsIPropertyBag enumerator returns a nsISimpleEnumerator whose
        // elements are nsIProperty objects. Calling QueryInterface for
        // nsIProperty on the object doesn't return to the caller an object that
        // is already queried to nsIProperty but do it just in case it is fixed
        // at some point.
        ip.data = { name, value: value.data, QueryInterface: qi };
        yield ip.data.QueryInterface(Ci.nsIProperty);
      }
    }
  },

  /**
   * See nsIPropertyBag.idl
   *
   * Note: returns null instead of throwing when the property doesn't exist to
   *       simplify code and to silence warnings in debug builds.
   */
  getProperty: function UpdatePatch_getProperty(name) {
    if (this._attrNames.includes(name)) {
      throw Components.Exception(
        "Illegal value '" +
          name +
          "' (attribute exists on nsIUpdatePatch) " +
          "when calling method: [nsIWritablePropertyBag::getProperty]",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    if (name in this._properties && this._properties[name].present) {
      return this._properties[name].data;
    }
    return null;
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIUpdatePatch",
    "nsIPropertyBag",
    "nsIWritablePropertyBag",
  ]),
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
  this._patches = [];
  this._properties = {};
  this.isCompleteUpdate = false;
  this.channel = "default";
  this.promptWaitTime = Services.prefs.getIntPref(
    PREF_APP_UPDATE_PROMPTWAITTIME,
    43200
  );
  this.unsupported = false;

  // Null <update>, assume this is a message container and do no
  // further initialization
  if (!update) {
    return;
  }

  for (let i = 0; i < update.childNodes.length; ++i) {
    let patchElement = update.childNodes.item(i);
    if (
      patchElement.nodeType != patchElement.ELEMENT_NODE ||
      patchElement.localName != "patch"
    ) {
      continue;
    }

    let patch;
    try {
      patch = new UpdatePatch(patchElement);
    } catch (e) {
      continue;
    }
    this._patches.push(patch);
  }

  if (!this._patches.length && !update.hasAttribute("unsupported")) {
    throw Components.Exception("", Cr.NS_ERROR_ILLEGAL_VALUE);
  }

  // Set the installDate value with the current time. If the update has an
  // installDate attribute this will be replaced with that value if it doesn't
  // equal 0.
  this.installDate = new Date().getTime();
  this.patchCount = this._patches.length;

  for (let i = 0; i < update.attributes.length; ++i) {
    let attr = update.attributes.item(i);
    if (attr.name == "xmlns" || attr.value == "undefined") {
      // Don't save the XML namespace or undefined values.
      // If an undefined value is saved to the xml file it will be a string when
      // it is read from the xml file.
      continue;
    } else if (attr.name == "detailsURL") {
      this.detailsURL = attr.value;
    } else if (attr.name == "installDate" && attr.value) {
      let val = parseInt(attr.value);
      if (val) {
        this.installDate = val;
      }
    } else if (attr.name == "errorCode" && attr.value) {
      let val = parseInt(attr.value);
      if (val) {
        // Set the value of |_errorCode| instead of |errorCode| since
        // selectedPatch won't be available at this point and normally the
        // nsIUpdatePatch will provide the errorCode.
        this._errorCode = val;
      }
    } else if (attr.name == "isCompleteUpdate") {
      this.isCompleteUpdate = attr.value == "true";
    } else if (attr.name == "promptWaitTime") {
      if (!isNaN(attr.value)) {
        this.promptWaitTime = parseInt(attr.value);
      }
    } else if (attr.name == "unsupported") {
      this.unsupported = attr.value == "true";
    } else {
      switch (attr.name) {
        case "appVersion":
        case "buildID":
        case "channel":
        case "displayVersion":
        case "elevationFailure":
        case "name":
        case "previousAppVersion":
        case "serviceURL":
        case "statusText":
        case "type":
          this[attr.name] = attr.value;
          break;
        default:
          if (!this._attrNames.includes(attr.name)) {
            // Set nsIPropertyBag properties that were read from the xml file.
            this.setProperty(attr.name, attr.value);
          }
          break;
      }
    }
  }

  if (!this.previousAppVersion) {
    this.previousAppVersion = Services.appinfo.version;
  }

  if (!this.elevationFailure) {
    this.elevationFailure = false;
  }

  if (!this.detailsURL) {
    try {
      // Try using a default details URL supplied by the distribution
      // if the update XML does not supply one.
      this.detailsURL = Services.urlFormatter.formatURLPref(
        PREF_APP_UPDATE_URL_DETAILS
      );
    } catch (e) {
      this.detailsURL = "";
    }
  }

  if (!this.displayVersion) {
    this.displayVersion = this.appVersion;
  }

  if (!this.name) {
    // When the update doesn't provide a name fallback to using
    // "<App Name> <Update App Version>"
    let brandBundle = Services.strings.createBundle(URI_BRAND_PROPERTIES);
    let appName = brandBundle.GetStringFromName("brandShortName");
    this.name = gUpdateBundle.formatStringFromName("updateName", [
      appName,
      this.displayVersion,
    ]);
  }
}
Update.prototype = {
  // nsIUpdate attribute names used to prevent nsIWritablePropertyBag from over
  // writing nsIUpdate attributes.
  _attrNames: [
    "appVersion",
    "buildID",
    "channel",
    "detailsURL",
    "displayVersion",
    "elevationFailure",
    "errorCode",
    "installDate",
    "isCompleteUpdate",
    "name",
    "previousAppVersion",
    "promptWaitTime",
    "serviceURL",
    "state",
    "statusText",
    "type",
    "unsupported",
  ],

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
   * active updates from the update manager for some reason but still have
   * the update.status file to work with.
   */
  _state: "",
  get state() {
    if (this.selectedPatch) {
      return this.selectedPatch.state;
    }
    return this._state;
  },
  set state(state) {
    if (this.selectedPatch) {
      this.selectedPatch.state = state;
    }
    this._state = state;
  },

  /**
   * See nsIUpdateService.idl
   *
   * We use a copy of the errorCode cached on this object in |_errorCode| only
   * when there is no selected patch, i.e. in the case when we could not load
   * active updates from the update manager for some reason but still have
   * the update.status file to work with.
   */
  _errorCode: 0,
  get errorCode() {
    if (this.selectedPatch) {
      return this.selectedPatch.errorCode;
    }
    return this._errorCode;
  },
  set errorCode(errorCode) {
    if (this.selectedPatch) {
      this.selectedPatch.errorCode = errorCode;
    }
    this._errorCode = errorCode;
  },

  /**
   * See nsIUpdateService.idl
   */
  get selectedPatch() {
    for (let i = 0; i < this.patchCount; ++i) {
      if (this._patches[i].selected) {
        return this._patches[i];
      }
    }
    return null;
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
    let update = updates.createElementNS(URI_UPDATE_NS, "update");
    update.setAttribute("appVersion", this.appVersion);
    update.setAttribute("buildID", this.buildID);
    update.setAttribute("channel", this.channel);
    update.setAttribute("detailsURL", this.detailsURL);
    update.setAttribute("displayVersion", this.displayVersion);
    update.setAttribute("installDate", this.installDate);
    update.setAttribute("isCompleteUpdate", this.isCompleteUpdate);
    update.setAttribute("name", this.name);
    update.setAttribute("previousAppVersion", this.previousAppVersion);
    update.setAttribute("promptWaitTime", this.promptWaitTime);
    update.setAttribute("serviceURL", this.serviceURL);
    update.setAttribute("type", this.type);

    if (this.statusText) {
      update.setAttribute("statusText", this.statusText);
    }
    if (this.unsupported) {
      update.setAttribute("unsupported", this.unsupported);
    }
    if (this.elevationFailure) {
      update.setAttribute("elevationFailure", this.elevationFailure);
    }

    for (let [name, value] of Object.entries(this._properties)) {
      if (value.present && !this._attrNames.includes(name)) {
        update.setAttribute(name, value.data);
      }
    }

    for (let i = 0; i < this.patchCount; ++i) {
      update.appendChild(this.getPatchAt(i).serialize(updates));
    }

    updates.documentElement.appendChild(update);
    return update;
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  setProperty: function Update_setProperty(name, value) {
    if (this._attrNames.includes(name)) {
      throw Components.Exception(
        "Illegal value '" +
          name +
          "' (attribute exists on nsIUpdate) " +
          "when calling method: [nsIWritablePropertyBag::setProperty]",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    this._properties[name] = { data: value, present: true };
  },

  /**
   * See nsIWritablePropertyBag.idl
   */
  deleteProperty: function Update_deleteProperty(name) {
    if (this._attrNames.includes(name)) {
      throw Components.Exception(
        "Illegal value '" +
          name +
          "' (attribute exists on nsIUpdate) " +
          "when calling method: [nsIWritablePropertyBag::deleteProperty]",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    if (name in this._properties) {
      this._properties[name].present = false;
    } else {
      throw Components.Exception("", Cr.NS_ERROR_FAILURE);
    }
  },

  /**
   * See nsIPropertyBag.idl
   *
   * Note: this only contains the nsIPropertyBag name value / pairs and not the
   *       nsIUpdate name / value pairs.
   */
  get enumerator() {
    return this.enumerate();
  },

  *enumerate() {
    // An nsISupportsInterfacePointer is used so creating an array using
    // Array.from will retain the QueryInterface for nsIProperty.
    let ip = Cc["@mozilla.org/supports-interface-pointer;1"].createInstance(
      Ci.nsISupportsInterfacePointer
    );
    let qi = ChromeUtils.generateQI(["nsIProperty"]);
    for (let [name, value] of Object.entries(this._properties)) {
      if (value.present && !this._attrNames.includes(name)) {
        // The nsIPropertyBag enumerator returns a nsISimpleEnumerator whose
        // elements are nsIProperty objects. Calling QueryInterface for
        // nsIProperty on the object doesn't return to the caller an object that
        // is already queried to nsIProperty but do it just in case it is fixed
        // at some point.
        ip.data = { name, value: value.data, QueryInterface: qi };
        yield ip.data.QueryInterface(Ci.nsIProperty);
      }
    }
  },

  /**
   * See nsIPropertyBag.idl
   * Note: returns null instead of throwing when the property doesn't exist to
   *       simplify code and to silence warnings in debug builds.
   */
  getProperty: function Update_getProperty(name) {
    if (this._attrNames.includes(name)) {
      throw Components.Exception(
        "Illegal value '" +
          name +
          "' (attribute exists on nsIUpdate) " +
          "when calling method: [nsIWritablePropertyBag::getProperty]",
        Cr.NS_ERROR_ILLEGAL_VALUE
      );
    }
    if (name in this._properties && this._properties[name].present) {
      return this._properties[name].data;
    }
    return null;
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIUpdate",
    "nsIPropertyBag",
    "nsIWritablePropertyBag",
  ]),
};

const UpdateServiceFactory = {
  _instance: null,
  createInstance(outer, iid) {
    if (outer != null) {
      throw Components.Exception("", Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this._instance == null
      ? (this._instance = new UpdateService())
      : this._instance;
  },
};

/**
 * UpdateService
 * A Service for managing the discovery and installation of software updates.
 * @constructor
 */
function UpdateService() {
  LOG("Creating UpdateService");
  // The observor notification to shut down the service must be before
  // profile-before-change since nsIUpdateManager uses profile-before-change
  // to shutdown and write the update xml files.
  Services.obs.addObserver(this, "quit-application");
  // This one call observes PREF_APP_UPDATE_LOG and PREF_APP_UPDATE_LOG_FILE
  Services.prefs.addObserver(PREF_APP_UPDATE_LOG, this);

  this._logStatus();
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
  observe: async function AUS_observe(subject, topic, data) {
    switch (topic) {
      case "post-update-processing":
        // This pref was not cleared out of profiles after it stopped being used
        // (Bug 1420514), so clear it out on the next update to avoid confusion
        // regarding its use.
        Services.prefs.clearUserPref("app.update.enabled");
        Services.prefs.clearUserPref("app.update.BITS.inTrialGroup");

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
        if (Services.appinfo.ID in APPID_TO_TOPIC) {
          Services.obs.removeObserver(
            this,
            APPID_TO_TOPIC[Services.appinfo.ID]
          );
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
        if (data == PREF_APP_UPDATE_LOG || data == PREF_APP_UPDATE_LOG_FILE) {
          gLogEnabled; // Assigning this before it is lazy-loaded is an error.
          gLogEnabled =
            Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG, false) ||
            Services.prefs.getBoolPref(PREF_APP_UPDATE_LOG_FILE, false);
        }
        if (data == PREF_APP_UPDATE_LOG_FILE) {
          gLogfileEnabled; // Assigning this before it is lazy-loaded is an
          // error.
          gLogfileEnabled = Services.prefs.getBoolPref(
            PREF_APP_UPDATE_LOG_FILE,
            false
          );
          if (gLogfileEnabled) {
            this._logStatus();
          }
        }
        break;
      case "quit-application":
        Services.obs.removeObserver(this, topic);
        Services.prefs.removeObserver(PREF_APP_UPDATE_LOG, this);

        if (AppConstants.platform == "win" && gUpdateMutexHandle) {
          // If we hold the update mutex, let it go!
          // The OS would clean this up sometime after shutdown,
          // but that would have no guarantee on timing.
          closeHandle(gUpdateMutexHandle);
          gUpdateMutexHandle = null;
        }
        if (this._retryTimer) {
          this._retryTimer.cancel();
        }

        // When downloading an update with nsIIncrementalDownload the download
        // is stopped when the quit-application observer notification is
        // received and networking hasn't started to shutdown. The download will
        // be resumed the next time the application starts. Downloads using
        // Windows BITS are not stopped since they don't require Firefox to be
        // running to perform the download.
        if (this._downloader && !this._downloader.usingBits) {
          this.stopDownload();
        }
        // Prevent leaking the downloader (bug 454964)
        this._downloader = null;
        // In case an update check is in progress.
        Cc["@mozilla.org/updates/update-checker;1"]
          .createInstance(Ci.nsIUpdateChecker)
          .stopCurrentCheck();

        if (gLogfileOutputStream) {
          gLogfileOutputStream.close();
        }
        break;
      case "test-close-handle-update-mutex":
        if (Cu.isInAutomation) {
          if (AppConstants.platform == "win" && gUpdateMutexHandle) {
            LOG("UpdateService:observe - closing mutex handle for testing");
            closeHandle(gUpdateMutexHandle);
            gUpdateMutexHandle = null;
          }
        }
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
  /* eslint-disable-next-line complexity */
  _postUpdateProcessing: function AUS__postUpdateProcessing() {
    if (this.disabledByPolicy) {
      // This function is a point when we can potentially enter the update
      // system, even with update disabled. Make sure that we do not continue
      // because update code can have side effects that are visible to the user
      // and give the impression that updates are enabled. For example, if we
      // can't write to the update directory, we might complain to the user that
      // update is broken and they should reinstall.
      return;
    }
    gUpdateFileWriteInfo = { phase: "startup", failure: false };
    if (!this.canCheckForUpdates) {
      LOG(
        "UpdateService:_postUpdateProcessing - unable to check for " +
          "updates... returning early"
      );
      return;
    }
    let status = readStatusFile(getReadyUpdateDir());

    if (!this.canApplyUpdates) {
      LOG(
        "UpdateService:_postUpdateProcessing - unable to apply " +
          "updates... returning early"
      );
      if (hasUpdateMutex()) {
        // If the update is present in the update directory somehow,
        // it would prevent us from notifying the user of further updates.
        cleanupActiveUpdates();
      }
      return;
    }

    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );

    let updates = [];
    if (um.readyUpdate) {
      updates.push(um.readyUpdate);
    }
    if (um.downloadingUpdate) {
      updates.push(um.downloadingUpdate);
    }

    if (status == STATE_NONE) {
      // A status of STATE_NONE in _postUpdateProcessing means that the
      // update.status file is present but there isn't an update in progress.
      // This isn't an expected state, so if we find ourselves in it, we want
      // to just clean things up to go back to a good state.
      if (!updates.length) {
        updates.push(new Update(null));
      }
      for (let update of updates) {
        update.state = STATE_FAILED;
        update.errorCode = ERR_UPDATE_STATE_NONE;
        update.statusText = gUpdateBundle.GetStringFromName("statusFailed");
      }
      let newStatus = STATE_FAILED + ": " + ERR_UPDATE_STATE_NONE;
      pingStateAndStatusCodes(updates[0], true, newStatus);
      cleanupActiveUpdates();
      return;
    }

    let channelChanged = updates => {
      for (let update of updates) {
        if (update.channel != UpdateUtils.UpdateChannel) {
          return true;
        }
      }
      return false;
    };
    if (channelChanged(updates)) {
      LOG(
        "UpdateService:_postUpdateProcessing - channel has changed, " +
          "reloading default preferences to workaround bug 802022"
      );
      // Workaround to get the distribution preferences loaded (Bug 774618).
      // This can be removed after bug 802022 is fixed. Now that this code runs
      // later during startup this code may no longer be necessary but it
      // shouldn't be removed until after bug 802022 is fixed.
      let prefSvc = Services.prefs.QueryInterface(Ci.nsIObserver);
      prefSvc.observe(null, "reload-default-prefs", null);
      if (channelChanged(updates)) {
        let channel = um.readyUpdate
          ? um.readyUpdate.channel
          : um.downloadingUpdate.channel;
        LOG(
          "UpdateService:_postUpdateProcessing - update channel is " +
            "different than application's channel, removing update. update " +
            "channel: " +
            channel +
            ", expected channel: " +
            UpdateUtils.UpdateChannel
        );
        // User switched channels, clear out the old active updates and remove
        // partial downloads
        for (let update of updates) {
          update.state = STATE_FAILED;
          update.errorCode = ERR_CHANNEL_CHANGE;
          update.statusText = gUpdateBundle.GetStringFromName("statusFailed");
        }
        let newStatus = STATE_FAILED + ": " + ERR_CHANNEL_CHANGE;
        pingStateAndStatusCodes(updates[0], true, newStatus);
        cleanupActiveUpdates();
        return;
      }
    }

    // Handle the case when the update is the same or older than the current
    // version and nsUpdateDriver.cpp skipped updating due to the version being
    // older than the current version. This also handles the general case when
    // an update is for an older version or the same version and same build ID.
    if (
      status == STATE_PENDING ||
      status == STATE_PENDING_SERVICE ||
      status == STATE_APPLIED ||
      status == STATE_APPLIED_SERVICE ||
      status == STATE_PENDING_ELEVATE ||
      status == STATE_DOWNLOADING
    ) {
      let tooOldUpdate;
      if (
        updateIsAtLeastAsOldAs(
          um.readyUpdate,
          Services.appinfo.version,
          Services.appinfo.appBuildID
        )
      ) {
        tooOldUpdate = um.readyUpdate;
      } else if (
        updateIsAtLeastAsOldAs(
          um.downloadingUpdate,
          Services.appinfo.version,
          Services.appinfo.appBuildID
        )
      ) {
        tooOldUpdate = um.downloadingUpdate;
      }
      if (tooOldUpdate) {
        LOG(
          "UpdateService:_postUpdateProcessing - removing update for older " +
            "application version or same application version with same build " +
            "ID. update application version: " +
            tooOldUpdate.appVersion +
            ", " +
            "application version: " +
            Services.appinfo.version +
            ", update " +
            "build ID: " +
            tooOldUpdate.buildID +
            ", application build ID: " +
            Services.appinfo.appBuildID
        );
        tooOldUpdate.state = STATE_FAILED;
        tooOldUpdate.statusText = gUpdateBundle.GetStringFromName(
          "statusFailed"
        );
        tooOldUpdate.errorCode = ERR_OLDER_VERSION_OR_SAME_BUILD;
        // This could be split out to report telemetry for each case.
        let newStatus = STATE_FAILED + ": " + ERR_OLDER_VERSION_OR_SAME_BUILD;
        pingStateAndStatusCodes(tooOldUpdate, true, newStatus);
        // Cleanup both updates regardless of which one is too old. It's
        // exceedingly unlikely that a user could end up in a state where one
        // update is acceptable and the other isn't. And it makes this function
        // considerably more complex to try to deal with that possibility.
        cleanupActiveUpdates();
        return;
      }
    }

    pingStateAndStatusCodes(
      status == STATE_DOWNLOADING ? um.downloadingUpdate : um.readyUpdate,
      true,
      status
    );
    if (um.downloadingUpdate || status == STATE_DOWNLOADING) {
      if (status == STATE_SUCCEEDED) {
        // If we successfully installed an update while we were downloading
        // another update, the downloading update is now a partial MAR for
        // a version that is no longer installed. We know that it's a partial
        // MAR without checking because we currently only download partial MARs
        // when an update has already been downloaded.
        LOG(
          "UpdateService:_postUpdateProcessing - removing downloading patch " +
            "because we installed a different patch before it finished" +
            "downloading."
        );
        cleanupDownloadingUpdate();
      } else {
        // Attempt to resume download
        if (um.downloadingUpdate) {
          LOG(
            "UpdateService:_postUpdateProcessing - resuming patch found in " +
              "downloading state"
          );
          let success = this.downloadUpdate(um.downloadingUpdate, true);
          if (!success) {
            cleanupDownloadingUpdate();
          }
        } else {
          LOG(
            "UpdateService:_postUpdateProcessing - Warning: found " +
              "downloading state, but no downloading patch"
          );
          // Put ourselves back in a good state.
          cleanupActiveUpdates();
        }
        if (status == STATE_DOWNLOADING) {
          // Done dealing with the downloading update, and there is no ready
          // update, so return early.
          return;
        }
      }
    }

    let update = um.readyUpdate;

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
      if (
        update &&
        (update.state == STATE_PENDING || update.state == STATE_PENDING_SERVICE)
      ) {
        LOG(
          "UpdateService:_postUpdateProcessing - patch found in applying " +
            "state for the first time"
        );
        update.state = STATE_APPLYING;
        um.saveUpdates();
      } else {
        // We get here even if we don't have an update object
        LOG(
          "UpdateService:_postUpdateProcessing - patch found in applying " +
            "state for the second time"
        );
        cleanupReadyUpdate();
      }
      return;
    }

    if (!update) {
      if (status != STATE_SUCCEEDED) {
        LOG(
          "UpdateService:_postUpdateProcessing - previous patch failed " +
            "and no patch available"
        );
        cleanupReadyUpdate();
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
      // Rotate the update logs so the update log isn't removed. By passing
      // false the patch directory won't be removed.
      cleanUpReadyUpdateDir(false);
    }

    if (status == STATE_SUCCEEDED) {
      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS)) {
        Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS);
      }
      update.statusText = gUpdateBundle.GetStringFromName("installSuccess");

      // The only time that update is not a reference to readyUpdate is when
      // readyUpdate is null.
      if (!um.readyUpdate) {
        um.readyUpdate = update;
      }

      // Done with this update. Clean it up.
      cleanupReadyUpdate();

      Services.prefs.setIntPref(PREF_APP_UPDATE_ELEVATE_ATTEMPTS, 0);
    } else if (status == STATE_PENDING_ELEVATE) {
      // In case the active-update.xml file is deleted.
      if (!update) {
        LOG(
          "UpdateService:_postUpdateProcessing - status is pending-elevate " +
            "but there isn't a ready update, removing update"
        );
        cleanupReadyUpdate();
      } else {
        let uri = "chrome://mozapps/content/update/updateElevation.xhtml";
        let features =
          "chrome,centerscreen,resizable=no,titlebar,toolbar=no,dialog=no";
        Services.ww.openWindow(null, uri, "Update:Elevation", features, null);
      }
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
      handleFallbackToCompleteUpdate(false);
    }
  },

  /**
   * Register an observer when the network comes online, so we can short-circuit
   * the app.update.interval when there isn't connectivity
   */
  _registerOnlineObserver: function AUS__registerOnlineObserver() {
    if (this._registeredOnlineObserver) {
      LOG(
        "UpdateService:_registerOnlineObserver - observer already registered"
      );
      return;
    }

    LOG(
      "UpdateService:_registerOnlineObserver - waiting for the network to " +
        "be online, then forcing another check"
    );

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

    LOG(
      "UpdateService:_offlineStatusChanged - network is online, forcing " +
        "another background check"
    );

    // the background checker is contained in notify
    this._attemptResume();
  },

  onCheckComplete: function AUS_onCheckComplete(request, updates) {
    this._selectAndInstallUpdate(updates);
  },

  onError: function AUS_onError(request, update) {
    LOG(
      "UpdateService:onError - error during background update. error code: " +
        update.errorCode +
        ", status text: " +
        update.statusText
    );

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
    let errCount = Services.prefs.getIntPref(
      PREF_APP_UPDATE_BACKGROUNDERRORS,
      0
    );

    // If we already have an update ready, we don't want to worry the user over
    // update check failures. As far as the user knows, the update status is
    // the status of the ready update. We don't want to confuse them by saying
    // that an update check failed.
    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (um.readyUpdate) {
      LOG(
        "UpdateService:onError - Ignoring error because another update is " +
          "ready."
      );
      return;
    }

    errCount++;
    Services.prefs.setIntPref(PREF_APP_UPDATE_BACKGROUNDERRORS, errCount);
    // Don't allow the preference to set a value greater than 20 for max errors.
    let maxErrors = Math.min(
      Services.prefs.getIntPref(PREF_APP_UPDATE_BACKGROUNDMAXERRORS, 10),
      20
    );

    if (errCount >= maxErrors) {
      LOG(
        "UpdateService:onError - notifying observers of error. " +
          "topic: update-error, status: check-attempts-exceeded"
      );
      Services.obs.notifyObservers(
        update,
        "update-error",
        "check-attempts-exceeded"
      );
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_GENERAL_ERROR_PROMPT);
    } else {
      LOG(
        "UpdateService:onError - notifying observers of error. " +
          "topic: update-error, status: check-attempt-failed"
      );
      Services.obs.notifyObservers(
        update,
        "update-error",
        "check-attempt-failed"
      );
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_GENERAL_ERROR_SILENT);
    }
  },

  /**
   * Called when a connection should be resumed
   */
  _attemptResume: function AUS_attemptResume() {
    LOG("UpdateService:_attemptResume");
    // If a download is in progress, then resume it.
    if (
      this._downloader &&
      this._downloader._patch &&
      this._downloader._patch.state == STATE_DOWNLOADING &&
      this._downloader._update
    ) {
      LOG(
        "UpdateService:_attemptResume - _patch.state: " +
          this._downloader._patch.state
      );
      let success = this.downloadUpdate(
        this._downloader._update,
        this._downloader.background
      );
      LOG("UpdateService:_attemptResume - downloadUpdate success: " + success);
      if (!success) {
        cleanupDownloadingUpdate();
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
    return this._checkForBackgroundUpdates(false);
  },

  // The suffix used for background update check telemetry histogram ID's.
  get _pingSuffix() {
    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (um.readyUpdate) {
      // Once an update has been downloaded, all later updates will be reported
      // to telemetry as subsequent updates. We move the first update into
      // readyUpdate as soon as the download is complete, so any update checks
      // after readyUpdate is no longer null are subsequent update checks.
      return AUSTLMY.SUBSEQUENT;
    }
    return this._isNotify ? AUSTLMY.NOTIFY : AUSTLMY.EXTERNAL;
  },

  /**
   * Checks for updates in the background.
   * @param   isNotify
   *          Whether or not a background update check was initiated by the
   *          application update timer notification.
   */
  _checkForBackgroundUpdates: function AUS__checkForBackgroundUpdates(
    isNotify
  ) {
    if (this.disabledByPolicy || this.manualUpdateOnly) {
      // Return immediately if we are disabled by policy. Otherwise, just the
      // telemetry we try to collect below can potentially trigger a restart
      // prompt if the update directory isn't writable. And we shouldn't be
      // telling the user about update failures if update is disabled.
      // See Bug 1599590.
      // Note that we exit unconditionally here if we are only doing manual
      // update checks, because manual update checking uses a completely
      // different code path (AppUpdater.jsm creates its own nsIUpdateChecker),
      // bypassing this function completely.
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_DISABLED_BY_POLICY);
      return false;
    }

    this._isNotify = isNotify;

    // Histogram IDs:
    // UPDATE_PING_COUNT_EXTERNAL
    // UPDATE_PING_COUNT_NOTIFY
    // UPDATE_PING_COUNT_SUBSEQUENT
    AUSTLMY.pingGeneric("UPDATE_PING_COUNT_" + this._pingSuffix, true, false);

    // Histogram IDs:
    // UPDATE_UNABLE_TO_APPLY_EXTERNAL
    // UPDATE_UNABLE_TO_APPLY_NOTIFY
    // UPDATE_UNABLE_TO_APPLY_SUBSEQUENT
    AUSTLMY.pingGeneric(
      "UPDATE_UNABLE_TO_APPLY_" + this._pingSuffix,
      getCanApplyUpdates(),
      true
    );
    // Histogram IDs:
    // UPDATE_CANNOT_STAGE_EXTERNAL
    // UPDATE_CANNOT_STAGE_NOTIFY
    // UPDATE_CANNOT_STAGE_SUBSEQUENT
    AUSTLMY.pingGeneric(
      "UPDATE_CANNOT_STAGE_" + this._pingSuffix,
      getCanStageUpdates(),
      true
    );
    if (AppConstants.platform == "win") {
      // Histogram IDs:
      // UPDATE_CAN_USE_BITS_EXTERNAL
      // UPDATE_CAN_USE_BITS_NOTIFY
      // UPDATE_CAN_USE_BITS_SUBSEQUENT
      AUSTLMY.pingGeneric(
        "UPDATE_CAN_USE_BITS_" + this._pingSuffix,
        getCanUseBits()
      );
    }
    // Histogram IDs:
    // UPDATE_INVALID_LASTUPDATETIME_EXTERNAL
    // UPDATE_INVALID_LASTUPDATETIME_NOTIFY
    // UPDATE_INVALID_LASTUPDATETIME_SUBSEQUENT
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_EXTERNAL
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_NOTIFY
    // UPDATE_LAST_NOTIFY_INTERVAL_DAYS_SUBSEQUENT
    AUSTLMY.pingLastUpdateTime(this._pingSuffix);
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_AUTO_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_AUTO_NOTIFY
    // UPDATE_NOT_PREF_UPDATE_AUTO_SUBSEQUENT
    UpdateUtils.getAppUpdateAutoEnabled().then(enabled => {
      AUSTLMY.pingGeneric(
        "UPDATE_NOT_PREF_UPDATE_AUTO_" + this._pingSuffix,
        enabled,
        true
      );
    });
    // Histogram IDs:
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_EXTERNAL
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_NOTIFY
    // UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_SUBSEQUENT
    AUSTLMY.pingBoolPref(
      "UPDATE_NOT_PREF_UPDATE_STAGING_ENABLED_" + this._pingSuffix,
      PREF_APP_UPDATE_STAGING_ENABLED,
      true,
      true
    );
    if (AppConstants.platform == "win" || AppConstants.platform == "macosx") {
      // Histogram IDs:
      // UPDATE_PREF_UPDATE_CANCELATIONS_EXTERNAL
      // UPDATE_PREF_UPDATE_CANCELATIONS_NOTIFY
      // UPDATE_PREF_UPDATE_CANCELATIONS_SUBSEQUENT
      AUSTLMY.pingIntPref(
        "UPDATE_PREF_UPDATE_CANCELATIONS_" + this._pingSuffix,
        PREF_APP_UPDATE_CANCELATIONS,
        0,
        0
      );
    }
    if (AppConstants.MOZ_MAINTENANCE_SERVICE) {
      // Histogram IDs:
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_EXTERNAL
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_NOTIFY
      // UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_SUBSEQUENT
      AUSTLMY.pingBoolPref(
        "UPDATE_NOT_PREF_UPDATE_SERVICE_ENABLED_" + this._pingSuffix,
        PREF_APP_UPDATE_SERVICE_ENABLED,
        true
      );
      // Histogram IDs:
      // UPDATE_PREF_SERVICE_ERRORS_EXTERNAL
      // UPDATE_PREF_SERVICE_ERRORS_NOTIFY
      // UPDATE_PREF_SERVICE_ERRORS_SUBSEQUENT
      AUSTLMY.pingIntPref(
        "UPDATE_PREF_SERVICE_ERRORS_" + this._pingSuffix,
        PREF_APP_UPDATE_SERVICE_ERRORS,
        0,
        0
      );
      if (AppConstants.platform == "win") {
        // Histogram IDs:
        // UPDATE_SERVICE_INSTALLED_EXTERNAL
        // UPDATE_SERVICE_INSTALLED_NOTIFY
        // UPDATE_SERVICE_INSTALLED_SUBSEQUENT
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_EXTERNAL
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_NOTIFY
        // UPDATE_SERVICE_MANUALLY_UNINSTALLED_SUBSEQUENT
        AUSTLMY.pingServiceInstallStatus(
          this._pingSuffix,
          isServiceInstalled()
        );
      }
    }

    // If a download is in progress or the patch has been staged do nothing.
    if (this.isDownloading) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADING);
      return false;
    }

    // Once we have downloaded a complete update, do not download further
    // updates until the complete update is installed. This is important,
    // because if we fall back from a partial update to a complete update,
    // it might be because of changes to the patch directory (which would cause
    // a failure to apply any partial MAR). So we really don't want to replace
    // a downloaded complete update with a downloaded partial update. And we
    // do not currently download complete updates if there is already a
    // readyUpdate available.
    var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (
      um.readyUpdate &&
      um.readyUpdate.selectedPatch &&
      um.readyUpdate.selectedPatch.type == "complete"
    ) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADED);
      return false;
    }

    // If we start downloading an update while the readyUpdate is staging, we
    // run the risk of eventually wanting to overwrite readyUpdate with the
    // downloadingUpdate while the readyUpdate is still staging. Then we would
    // have to have a weird intermediate state where the downloadingUpdate has
    // finished downloading, but can't be moved yet. It's simpler to just not
    // start a new update if the old one is still staging.
    if (gStagingInProgress) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_IS_DOWNLOADED);
      return false;
    }

    let validUpdateURL = true;
    this.backgroundChecker
      .getUpdateURL(false)
      .catch(e => {
        validUpdateURL = false;
      })
      .then(() => {
        // The following checks are done here so they can be differentiated from
        // foreground checks.
        if (!UpdateUtils.OSVersion) {
          AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_OS_VERSION);
        } else if (!UpdateUtils.ABI) {
          AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_OS_ABI);
        } else if (!validUpdateURL) {
          AUSTLMY.pingCheckCode(
            this._pingSuffix,
            AUSTLMY.CHK_INVALID_DEFAULT_URL
          );
        } else if (!hasUpdateMutex()) {
          AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_NO_MUTEX);
        } else if (isOtherInstanceRunning()) {
          AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_OTHER_INSTANCE);
        } else if (!this.canCheckForUpdates) {
          AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNABLE_TO_CHECK);
        }

        this.backgroundChecker.checkForUpdates(this, false);
      });
    return true;
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
    if (!updates.length) {
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
      if (
        vc.compare(aUpdate.appVersion, Services.appinfo.version) < 0 ||
        (vc.compare(aUpdate.appVersion, Services.appinfo.version) == 0 &&
          aUpdate.buildID == Services.appinfo.appBuildID)
      ) {
        LOG(
          "UpdateService:selectUpdate - skipping update because the " +
            "update's application version is less than the current " +
            "application version"
        );
        lastCheckCode = AUSTLMY.CHK_UPDATE_PREVIOUS_VERSION;
        return;
      }

      switch (aUpdate.type) {
        case "major":
          if (!majorUpdate) {
            majorUpdate = aUpdate;
          } else if (
            vc.compare(majorUpdate.appVersion, aUpdate.appVersion) <= 0
          ) {
            majorUpdate = aUpdate;
          }
          break;
        case "minor":
          if (!minorUpdate) {
            minorUpdate = aUpdate;
          } else if (
            vc.compare(minorUpdate.appVersion, aUpdate.appVersion) <= 0
          ) {
            minorUpdate = aUpdate;
          }
          break;
        default:
          LOG(
            "UpdateService:selectUpdate - skipping unknown update type: " +
              aUpdate.type
          );
          lastCheckCode = AUSTLMY.CHK_UPDATE_INVALID_TYPE;
          break;
      }
    });

    let update = minorUpdate || majorUpdate;
    if (AppConstants.platform == "macosx" && update) {
      if (getElevationRequired()) {
        let installAttemptVersion = Services.prefs.getCharPref(
          PREF_APP_UPDATE_ELEVATE_VERSION,
          null
        );
        if (vc.compare(installAttemptVersion, update.appVersion) != 0) {
          Services.prefs.setCharPref(
            PREF_APP_UPDATE_ELEVATE_VERSION,
            update.appVersion
          );
          if (
            Services.prefs.prefHasUserValue(PREF_APP_UPDATE_CANCELATIONS_OSX)
          ) {
            Services.prefs.clearUserPref(PREF_APP_UPDATE_CANCELATIONS_OSX);
          }
          if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_ELEVATE_NEVER)) {
            Services.prefs.clearUserPref(PREF_APP_UPDATE_ELEVATE_NEVER);
          }
        } else {
          let numCancels = Services.prefs.getIntPref(
            PREF_APP_UPDATE_CANCELATIONS_OSX,
            0
          );
          let rejectedVersion = Services.prefs.getCharPref(
            PREF_APP_UPDATE_ELEVATE_NEVER,
            ""
          );
          let maxCancels = Services.prefs.getIntPref(
            PREF_APP_UPDATE_CANCELATIONS_OSX_MAX,
            DEFAULT_CANCELATIONS_OSX_MAX
          );
          if (numCancels >= maxCancels) {
            LOG(
              "UpdateService:selectUpdate - the user requires elevation to " +
                "install this update, but the user has exceeded the max " +
                "number of elevation attempts."
            );
            update.elevationFailure = true;
            AUSTLMY.pingCheckCode(
              this._pingSuffix,
              AUSTLMY.CHK_ELEVATION_DISABLED_FOR_VERSION
            );
          } else if (vc.compare(rejectedVersion, update.appVersion) == 0) {
            LOG(
              "UpdateService:selectUpdate - the user requires elevation to " +
                "install this update, but elevation is disabled for this " +
                "version."
            );
            update.elevationFailure = true;
            AUSTLMY.pingCheckCode(
              this._pingSuffix,
              AUSTLMY.CHK_ELEVATION_OPTOUT_FOR_VERSION
            );
          } else {
            LOG(
              "UpdateService:selectUpdate - the user requires elevation to " +
                "install the update."
            );
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
  _selectAndInstallUpdate: async function AUS__selectAndInstallUpdate(updates) {
    // Return early if there's an active update. The user is already aware and
    // is downloading or performed some user action to prevent notification.
    var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (um.downloadingUpdate) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_HAS_ACTIVEUPDATE);
      return;
    }

    if (this.disabledByPolicy) {
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_DISABLED_BY_POLICY);
      LOG(
        "UpdateService:_selectAndInstallUpdate - not prompting because " +
          "update is disabled"
      );
      return;
    }

    var update = this.selectUpdate(updates);
    if (!update || update.elevationFailure) {
      return;
    }

    if (update.unsupported) {
      LOG(
        "UpdateService:_selectAndInstallUpdate - update not supported for " +
          "this system. Notifying observers. topic: update-available, " +
          "status: unsupported"
      );
      Services.obs.notifyObservers(update, "update-available", "unsupported");
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_UNSUPPORTED);
      return;
    }

    if (!getCanApplyUpdates()) {
      LOG(
        "UpdateService:_selectAndInstallUpdate - the user is unable to " +
          "apply updates... prompting. Notifying observers. " +
          "topic: update-available, status: cant-apply"
      );
      Services.obs.notifyObservers(null, "update-available", "cant-apply");
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
    let updateAuto = await UpdateUtils.getAppUpdateAutoEnabled();
    if (!updateAuto) {
      LOG(
        "UpdateService:_selectAndInstallUpdate - prompting because silent " +
          "install is disabled. Notifying observers. topic: update-available, " +
          "status: show-prompt"
      );
      AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_SHOWPROMPT_PREF);
      Services.obs.notifyObservers(update, "update-available", "show-prompt");
      return;
    }

    LOG("UpdateService:_selectAndInstallUpdate - download the update");
    let success = this.downloadUpdate(update, true);
    if (!success) {
      cleanupDownloadingUpdate();
    }
    AUSTLMY.pingCheckCode(this._pingSuffix, AUSTLMY.CHK_DOWNLOAD_UPDATE);
  },

  /**
   * The Checker used for background update checks.
   */
  _backgroundChecker: null,

  /**
   * See nsIUpdateService.idl
   */
  get backgroundChecker() {
    if (!this._backgroundChecker) {
      this._backgroundChecker = new Checker();
    }
    return this._backgroundChecker;
  },

  get disabledForTesting() {
    let marionetteRunning = false;

    if ("nsIMarionette" in Ci) {
      marionetteRunning = Cc["@mozilla.org/remote/marionette;1"].createInstance(
        Ci.nsIMarionette
      ).running;
    }

    return (
      (Cu.isInAutomation || marionetteRunning || RemoteAgent.listening) &&
      Services.prefs.getBoolPref(PREF_APP_UPDATE_DISABLEDFORTESTING, false)
    );
  },

  get disabledByPolicy() {
    return (
      (Services.policies && !Services.policies.isAllowed("appUpdate")) ||
      this.disabledForTesting
    );
  },

  /**
   * See nsIUpdateService.idl
   */
  get manualUpdateOnly() {
    return (
      Services.policies && !Services.policies.isAllowed("autoAppUpdateChecking")
    );
  },

  /**
   * See nsIUpdateService.idl
   */
  get canCheckForUpdates() {
    if (this.disabledByPolicy) {
      LOG(
        "UpdateService.canCheckForUpdates - unable to automatically check " +
          "for updates, the option has been disabled by the administrator."
      );
      return false;
    }

    // If we don't know the binary platform we're updating, we can't update.
    if (!UpdateUtils.ABI) {
      LOG(
        "UpdateService.canCheckForUpdates - unable to check for updates, " +
          "unknown ABI"
      );
      return false;
    }

    // If we don't know the OS version we're updating, we can't update.
    if (!UpdateUtils.OSVersion) {
      LOG(
        "UpdateService.canCheckForUpdates - unable to check for updates, " +
          "unknown OS version"
      );
      return false;
    }

    if (!hasUpdateMutex()) {
      LOG(
        "UpdateService.canCheckForUpdates - unable to check for updates, " +
          "unable to acquire update mutex"
      );
      return false;
    }

    if (isOtherInstanceRunning()) {
      // This doesn't block update checks, but we will have to wait until either
      // the other instance is gone or we time out waiting for it.
      LOG(
        "UpdateService.canCheckForUpdates - another instance is holding the " +
          "lock, will need to wait for it prior to checking for updates"
      );
    }

    LOG("UpdateService.canCheckForUpdates - able to check for updates");
    return true;
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
    return (
      getCanApplyUpdates() && hasUpdateMutex() && !isOtherInstanceRunning()
    );
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
    return !hasUpdateMutex() || isOtherInstanceRunning();
  },

  /**
   * A set of download listeners to be notified by this._downloader when it
   * receives nsIRequestObserver or nsIProgressEventSink method calls.
   *
   * These are stored on the UpdateService rather than on the Downloader,
   * because they ought to persist across multiple Downloader instances.
   */
  _downloadListeners: new Set(),

  /**
   * See nsIUpdateService.idl
   */
  addDownloadListener: function AUS_addDownloadListener(listener) {
    let oldSize = this._downloadListeners.size;
    this._downloadListeners.add(listener);

    if (this._downloadListeners.size == oldSize) {
      LOG(
        "UpdateService:addDownloadListener - Warning: Didn't add duplicate " +
          "listener"
      );
      return;
    }

    if (this._downloader) {
      this._downloader.onDownloadListenerAdded();
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  removeDownloadListener: function AUS_removeDownloadListener(listener) {
    let elementRemoved = this._downloadListeners.delete(listener);

    if (!elementRemoved) {
      LOG(
        "UpdateService:removeDownloadListener - Warning: Didn't remove " +
          "non-existent listener"
      );
      return;
    }

    if (this._downloader) {
      this._downloader.onDownloadListenerRemoved();
    }
  },

  /**
   * Returns a boolean indicating whether there are any download listeners
   */
  get hasDownloadListeners() {
    return !!this._downloadListeners.length;
  },

  /*
   * Calls the provided function once with each download listener that is
   * currently registered.
   */
  forEachDownloadListener: function AUS_forEachDownloadListener(fn) {
    // Make a shallow copy in case listeners remove themselves.
    let listeners = new Set(this._downloadListeners);
    listeners.forEach(fn);
  },

  /**
   * See nsIUpdateService.idl
   */
  downloadUpdate: function AUS_downloadUpdate(update, background) {
    if (!update) {
      throw Components.Exception("", Cr.NS_ERROR_NULL_POINTER);
    }

    // Don't download the update if the update's version is less than the
    // current application's version or the update's version is the same as the
    // application's version and the build ID is the same as the application's
    // build ID. If we already have an update ready, we want to apply those
    // same checks against the version of the ready update, so that we don't
    // download an update that isn't newer than the one we already have.
    if (
      updateIsAtLeastAsOldAs(
        update,
        Services.appinfo.version,
        Services.appinfo.appBuildID
      )
    ) {
      LOG(
        "UpdateService:downloadUpdate - canceling download of update since " +
          "it is for an earlier or same application version and build ID.\n" +
          "current application version: " +
          Services.appinfo.version +
          "\n" +
          "update application version : " +
          update.appVersion +
          "\n" +
          "current build ID: " +
          Services.appinfo.appBuildID +
          "\n" +
          "update build ID : " +
          update.buildID
      );
      cleanupDownloadingUpdate();
      return false;
    }
    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (
      um.readyUpdate &&
      um.readyUpdate.appVersion &&
      um.readyUpdate.buildID &&
      updateIsAtLeastAsOldAs(
        update,
        um.readyUpdate.appVersion,
        um.readyUpdate.buildID
      )
    ) {
      LOG(
        "UpdateService:downloadUpdate - not downloading update because the " +
          "update that's already been downloaded is the same version or " +
          "newer.\n" +
          "currently downloaded update application version: " +
          um.readyUpdate.appVersion +
          "\n" +
          "available update application version : " +
          update.appVersion +
          "\n" +
          "currently downloaded update build ID: " +
          um.readyUpdate.buildID +
          "\n" +
          "available update build ID : " +
          update.buildID
      );
      cleanupDownloadingUpdate();
      return false;
    }

    // If a download request is in progress vs. a download ready to resume
    if (this.isDownloading) {
      if (update.isCompleteUpdate == this._downloader.isCompleteUpdate) {
        LOG(
          "UpdateService:downloadUpdate - no support for downloading more " +
            "than one update at a time"
        );
        this._downloader.background = background;
        return true;
      }
      this._downloader.cancel();
    }
    this._downloader = new Downloader(background, this);
    return this._downloader.downloadUpdate(update);
  },

  /**
   * See nsIUpdateService.idl
   */
  stopDownload: function AUS_stopDownload() {
    if (this.isDownloading) {
      this._downloader.cancel();
    } else if (this._retryTimer) {
      // Download status is still considered as 'downloading' during retry.
      // We need to cancel both retry and download at this stage.
      this._retryTimer.cancel();
      this._retryTimer = null;
      if (this._downloader) {
        this._downloader.cancel();
      }
    }
    this._downloader = null;
  },

  /**
   * See nsIUpdateService.idl
   */
  get isDownloading() {
    return this._downloader && this._downloader.isBusy;
  },

  _logStatus: function AUS__logStatus() {
    if (!gLogEnabled) {
      return;
    }
    if (this.disabledByPolicy) {
      LOG("Current UpdateService status: disabledByPolicy");
      // Return early if UpdateService is disabled by policy. Otherwise some of
      // the getters we call to display status information may discover that the
      // update directory is not writable, which automatically results in the
      // permissions being fixed. Which we shouldn't really be doing if update
      // is disabled by policy.
      return;
    }
    LOG("Logging current UpdateService status:");
    // These getters print their own logging
    this.canCheckForUpdates;
    this.canApplyUpdates;
    this.canStageUpdates;
    LOG("Elevation required: " + this.elevationRequired);
    LOG(
      "Other instance of the application currently running: " +
        this.isOtherInstanceHandlingUpdates
    );
    LOG("Downloading: " + !!this.isDownloading);
    if (this._downloader && this._downloader.isBusy) {
      LOG("Downloading complete update: " + this._downloader.isCompleteUpdate);
      LOG("Downloader using BITS: " + this._downloader.usingBits);
      if (this._downloader._patch) {
        // This will print its own logging
        this._downloader._canUseBits(this._downloader._patch);

        // Downloader calls QueryInterface(Ci.nsIWritablePropertyBag) on
        // its _patch member as soon as it is assigned, so no need to do so
        // again here.
        let bitsResult = this._downloader._patch.getProperty("bitsResult");
        if (bitsResult != null) {
          LOG("Patch BITS result: " + bitsResult);
        }
        let internalResult = this._downloader._patch.getProperty(
          "internalResult"
        );
        if (internalResult != null) {
          LOG("Patch nsIIncrementalDownload result: " + internalResult);
        }
      }
    }
    LOG("End of UpdateService status");
  },

  classID: UPDATESERVICE_CID,

  _xpcom_factory: UpdateServiceFactory,
  QueryInterface: ChromeUtils.generateQI([
    "nsIApplicationUpdateService",
    "nsIUpdateCheckListener",
    "nsITimerCallback",
    "nsIObserver",
  ]),
};

/**
 * A service to manage active and past updates.
 * @constructor
 */
function UpdateManager() {
  // Load the active-update.xml file to see if there is an active update.
  let activeUpdates = this._loadXMLFileIntoArray(FILE_ACTIVE_UPDATE_XML);
  if (activeUpdates.length) {
    // Set the active update directly on the var used to cache the value.
    this._readyUpdate = activeUpdates[0];
    if (activeUpdates.length >= 2) {
      this._downloadingUpdate = activeUpdates[1];
    }
    let status = readStatusFile(getReadyUpdateDir());
    // This check is performed here since UpdateService:_postUpdateProcessing
    // won't be called when there isn't an update.status file.
    if (status == STATE_NONE) {
      // Under some edgecases such as Windows system restore the
      // active-update.xml will contain a pending update without the status
      // file. To recover from this situation clean the updates dir and move
      // the active update to the update history.
      this._readyUpdate.state = STATE_FAILED;
      this._readyUpdate.errorCode = ERR_UPDATE_STATE_NONE;
      this._readyUpdate.statusText = gUpdateBundle.GetStringFromName(
        "statusFailed"
      );
      let newStatus = STATE_FAILED + ": " + ERR_UPDATE_STATE_NONE;
      pingStateAndStatusCodes(this._readyUpdate, true, newStatus);
      this.addUpdateToHistory(this._readyUpdate);
      this._readyUpdate = null;
      this.saveUpdates();
      cleanUpReadyUpdateDir();
      cleanUpDownloadingUpdateDir();
    } else if (status == STATE_DOWNLOADING) {
      // The first update we read out of activeUpdates may not be the ready
      // update, it may be the downloading update.
      if (this._downloadingUpdate) {
        // If the first update we read is a downloading update, it's
        // unexpected to have read another active update. That would seem to
        // indicate that we were downloading two updates at once, which we don't
        // do.
        LOG(
          "UpdateManager:UpdateManager - Warning: Found and discarded a " +
            "second downloading update."
        );
      }
      this._downloadingUpdate = this._readyUpdate;
      this._readyUpdate = null;
    }
  }
}
UpdateManager.prototype = {
  /**
   * The nsIUpdate object for the update that has been downloaded.
   */
  _readyUpdate: null,

  /**
   * The nsIUpdate object for the update currently being downloaded.
   */
  _downloadingUpdate: null,

  /**
   * Whether the update history stored in _updates has changed since it was
   * loaded.
   */
  _updatesDirty: false,

  /**
   * See nsIObserver.idl
   */
  observe: function UM_observe(subject, topic, data) {
    // Hack to be able to run and cleanup tests by reloading the update data.
    if (topic == "um-reload-update-data") {
      if (!Cu.isInAutomation) {
        return;
      }
      if (this._updatesXMLSaver) {
        this._updatesXMLSaver.disarm();
      }

      let updates = [];
      this._updatesDirty = true;
      this._readyUpdate = null;
      this._downloadingUpdate = null;
      if (data != "skip-files") {
        let activeUpdates = this._loadXMLFileIntoArray(FILE_ACTIVE_UPDATE_XML);
        if (activeUpdates.length) {
          this._readyUpdate = activeUpdates[0];
          if (activeUpdates.length >= 2) {
            this._downloadingUpdate = activeUpdates[1];
          }
          if (readStatusFile(getReadyUpdateDir()) == STATE_DOWNLOADING) {
            this._downloadingUpdate = this._readyUpdate;
            this._readyUpdate = null;
          }
        }
        updates = this._loadXMLFileIntoArray(FILE_UPDATES_XML);
      }
      this._updatesCache = updates;
    }
  },

  /**
   * Loads an updates.xml formatted file into an array of nsIUpdate items.
   * @param   fileName
   *          The file name in the updates directory to load.
   * @return  The array of nsIUpdate items held in the file.
   */
  _loadXMLFileIntoArray: function UM__loadXMLFileIntoArray(fileName) {
    let updates = [];
    let file = getUpdateFile([fileName]);
    if (!file.exists()) {
      LOG(
        "UpdateManager:_loadXMLFileIntoArray - XML file does not exist. " +
          "path: " +
          file.path
      );
      return updates;
    }

    // Open the active-update.xml file with both read and write access so
    // opening it will fail if it isn't possible to also write to the file. When
    // opening it fails it means that it isn't possible to update and the code
    // below will return early without loading the active-update.xml. This will
    // also make it so notifications to update manually will still be shown.
    let mode =
      fileName == FILE_ACTIVE_UPDATE_XML
        ? FileUtils.MODE_RDWR
        : FileUtils.MODE_RDONLY;
    let fileStream = Cc[
      "@mozilla.org/network/file-input-stream;1"
    ].createInstance(Ci.nsIFileInputStream);
    try {
      fileStream.init(file, mode, FileUtils.PERMS_FILE, 0);
    } catch (e) {
      LOG(
        "UpdateManager:_loadXMLFileIntoArray - error initializing file " +
          "stream. Exception: " +
          e
      );
      fixUpdateDirectoryPermissions();
      return updates;
    }
    try {
      var parser = new DOMParser();
      var doc = parser.parseFromStream(
        fileStream,
        "UTF-8",
        fileStream.available(),
        "text/xml"
      );

      var updateCount = doc.documentElement.childNodes.length;
      for (var i = 0; i < updateCount; ++i) {
        var updateElement = doc.documentElement.childNodes.item(i);
        if (
          updateElement.nodeType != updateElement.ELEMENT_NODE ||
          updateElement.localName != "update"
        ) {
          continue;
        }

        let update;
        try {
          update = new Update(updateElement);
        } catch (e) {
          LOG("UpdateManager:_loadXMLFileIntoArray - invalid update");
          continue;
        }
        updates.push(update);
      }
    } catch (ex) {
      LOG(
        "UpdateManager:_loadXMLFileIntoArray - error constructing update " +
          "list. Exception: " +
          ex
      );
    }
    fileStream.close();
    if (!updates.length) {
      LOG(
        "UpdateManager:_loadXMLFileIntoArray - update xml file " +
          fileName +
          " exists but doesn't contain any updates"
      );
      // The file exists but doesn't contain any updates so remove it.
      try {
        file.remove(false);
      } catch (e) {
        LOG(
          "UpdateManager:_loadXMLFileIntoArray - error removing " +
            fileName +
            " file. Exception: " +
            e
        );
      }
    }
    return updates;
  },

  /**
   * Loads the update history from the updates.xml file into a cache.
   */
  _getUpdates() {
    if (!this._updatesCache) {
      this._updatesCache = this._loadXMLFileIntoArray(FILE_UPDATES_XML);
    }
    return this._updatesCache;
  },

  /**
   * See nsIUpdateService.idl
   */
  getUpdateAt: function UM_getUpdateAt(aIndex) {
    return this._getUpdates()[aIndex];
  },

  /**
   * See nsIUpdateService.idl
   */
  getUpdateCount() {
    return this._getUpdates().length;
  },

  /**
   * See nsIUpdateService.idl
   */
  get readyUpdate() {
    return this._readyUpdate;
  },
  set readyUpdate(aUpdate) {
    this._readyUpdate = aUpdate;
  },

  /**
   * See nsIUpdateService.idl
   */
  get downloadingUpdate() {
    return this._downloadingUpdate;
  },
  set downloadingUpdate(aUpdate) {
    this._downloadingUpdate = aUpdate;
  },

  /**
   * See nsIUpdateService.idl
   */
  addUpdateToHistory(aUpdate) {
    this._updatesDirty = true;
    let updates = this._getUpdates();
    updates.unshift(aUpdate);
    // Limit the update history to 10 updates.
    updates.splice(10);
  },

  /**
   * Serializes an array of updates to an XML file or removes the file if the
   * array length is 0.
   * @param   updates
   *          An array of nsIUpdate objects
   * @param   fileName
   *          The file name in the updates directory to write to.
   * @return  true on success, false on error
   */
  _writeUpdatesToXMLFile: async function UM__writeUpdatesToXMLFile(
    updates,
    fileName
  ) {
    let file;
    try {
      file = getUpdateFile([fileName]);
    } catch (e) {
      LOG(
        "UpdateManager:_writeUpdatesToXMLFile - Unable to get XML file - " +
          "Exception: " +
          e
      );
      return false;
    }
    if (!updates.length) {
      LOG(
        "UpdateManager:_writeUpdatesToXMLFile - no updates to write. " +
          "removing file: " +
          file.path
      );
      try {
        await IOUtils.remove(file.path);
      } catch (e) {
        LOG(
          "UpdateManager:_writeUpdatesToXMLFile - Delete file exception: " + e
        );
        return false;
      }
      return true;
    }

    const EMPTY_UPDATES_DOCUMENT_OPEN =
      '<?xml version="1.0"?><updates xmlns="' + URI_UPDATE_NS + '">';
    const EMPTY_UPDATES_DOCUMENT_CLOSE = "</updates>";
    try {
      var parser = new DOMParser();
      var doc = parser.parseFromString(
        EMPTY_UPDATES_DOCUMENT_OPEN + EMPTY_UPDATES_DOCUMENT_CLOSE,
        "text/xml"
      );

      for (var i = 0; i < updates.length; ++i) {
        doc.documentElement.appendChild(updates[i].serialize(doc));
      }

      var xml =
        EMPTY_UPDATES_DOCUMENT_OPEN +
        doc.documentElement.innerHTML +
        EMPTY_UPDATES_DOCUMENT_CLOSE;
      // If the destination file existed and is removed while the following is
      // being performed the copy of the tmp file to the destination file will
      // fail.
      await IOUtils.writeUTF8(file.path, xml, {
        tmpPath: file.path + ".tmp",
      });
      await IOUtils.setPermissions(file.path, FileUtils.PERMS_FILE);
    } catch (e) {
      LOG("UpdateManager:_writeUpdatesToXMLFile - Exception: " + e);
      return false;
    }
    return true;
  },

  _updatesXMLSaver: null,
  _updatesXMLSaverCallback: null,
  /**
   * See nsIUpdateService.idl
   */
  saveUpdates: function UM_saveUpdates() {
    if (!this._updatesXMLSaver) {
      this._updatesXMLSaverCallback = () => this._updatesXMLSaver.finalize();

      this._updatesXMLSaver = new DeferredTask(
        () => this._saveUpdatesXML(),
        XML_SAVER_INTERVAL_MS
      );
      AsyncShutdown.profileBeforeChange.addBlocker(
        "UpdateManager: writing update xml data",
        this._updatesXMLSaverCallback
      );
    } else {
      this._updatesXMLSaver.disarm();
    }

    this._updatesXMLSaver.arm();
  },

  /**
   * Saves the active-updates.xml and updates.xml when the updates history has
   * been modified files.
   */
  _saveUpdatesXML: function UM__saveUpdatesXML() {
    // This mechanism for how we store the updates might seem a bit odd, since,
    // if only one update is stored, we don't know if it's the ready update or
    // the downloading update. However, we can determine which it is by reading
    // update.status. If we read STATE_DOWNLOADING, it must be a downloading
    // update and otherwise it's a ready update. This method has the additional
    // advantage of requiring no migration from when we used to only store a
    // single active update.
    let updates = [];
    if (this._readyUpdate) {
      updates.push(this._readyUpdate);
    }
    if (this._downloadingUpdate) {
      updates.push(this._downloadingUpdate);
    }

    // The active update stored in the active-update.xml file will change during
    // the lifetime of an active update and the file should always be updated
    // when saveUpdates is called.
    let promises = [];
    promises[0] = this._writeUpdatesToXMLFile(
      updates,
      FILE_ACTIVE_UPDATE_XML
    ).then(wroteSuccessfully =>
      handleCriticalWriteResult(wroteSuccessfully, FILE_ACTIVE_UPDATE_XML)
    );
    // The update history stored in the updates.xml file should only need to be
    // updated when an active update has been added to it in which case
    // |_updatesDirty| will be true.
    if (this._updatesDirty) {
      this._updatesDirty = false;
      promises[1] = this._writeUpdatesToXMLFile(
        this._getUpdates(),
        FILE_UPDATES_XML
      ).then(wroteSuccessfully =>
        handleCriticalWriteResult(wroteSuccessfully, FILE_UPDATES_XML)
      );
    }
    return Promise.all(promises);
  },

  /**
   * See nsIUpdateService.idl
   */
  refreshUpdateStatus: async function UM_refreshUpdateStatus() {
    gStagingInProgress = false;

    var update = this._readyUpdate;
    if (!update) {
      return;
    }

    var status = readStatusFile(getReadyUpdateDir());
    pingStateAndStatusCodes(update, false, status);
    var parts = status.split(":");
    update.state = parts[0];
    if (update.state == STATE_FAILED && parts[1]) {
      update.errorCode = parseInt(parts[1]);
    }

    // Rotate the update logs so the update log isn't removed if a complete
    // update is downloaded. By passing false the patch directory won't be
    // removed.
    cleanUpReadyUpdateDir(false);

    if (update.state == STATE_FAILED && parts[1]) {
      if (
        parts[1] == DELETE_ERROR_STAGING_LOCK_FILE ||
        parts[1] == UNEXPECTED_STAGING_ERROR
      ) {
        update.state = getBestPendingState();
        writeStatusFile(getReadyUpdateDir(), update.state);
      } else if (!handleUpdateFailure(update, parts[1])) {
        handleFallbackToCompleteUpdate(true);
      }
    }
    if (update.state == STATE_APPLIED && shouldUseService()) {
      writeStatusFile(
        getReadyUpdateDir(),
        (update.state = STATE_APPLIED_SERVICE)
      );
    }

    // Now that the active update's properties have been updated write the
    // active-update.xml to disk. Since there have been no changes to the update
    // history the updates.xml will not be written to disk.
    this.saveUpdates();

    // Send an observer notification which the app update doorhanger uses to
    // display a restart notification after any langpacks have staged.
    promiseLangPacksUpdated(update).then(() => {
      LOG(
        "UpdateManager:refreshUpdateStatus - Notifying observers that " +
          "the update was staged. topic: update-staged, status: " +
          update.state
      );
      Services.obs.notifyObservers(update, "update-staged", update.state);
    });
  },

  /**
   * See nsIUpdateService.idl
   */
  elevationOptedIn: function UM_elevationOptedIn() {
    // The user has been been made aware that the update requires elevation.
    let update = this._readyUpdate;
    if (!update) {
      return;
    }
    let status = readStatusFile(getReadyUpdateDir());
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
      writeStatusFile(getReadyUpdateDir(), STATE_PENDING);
    }
  },

  /**
   * See nsIUpdateService.idl
   */
  cleanupDownloadingUpdate: function UM_cleanupDownloadingUpdate() {
    cleanupDownloadingUpdate();
  },

  /**
   * See nsIUpdateService.idl
   */
  cleanupReadyUpdate: function UM_cleanupReadyUpdate() {
    cleanupReadyUpdate();
  },

  classID: Components.ID("{093C2356-4843-4C65-8709-D7DBCBBE7DFB}"),
  QueryInterface: ChromeUtils.generateQI(["nsIUpdateManager", "nsIObserver"]),
};

/**
 * Checker
 * Checks for new Updates
 * @constructor
 */
function Checker() {}
Checker.prototype = {
  /**
   * The XMLHttpRequest object that performs the connection.
   */
  _request: null,

  /**
   * The nsIUpdateCheckListener callback
   */
  _callback: null,

  _getCanMigrate: function UC__getCanMigrate() {
    if (AppConstants.platform != "win") {
      return false;
    }

    // The first element of the array is whether the build target is 32 or 64
    // bit and the third element of the array is whether the client's Windows OS
    // system processor is 32 or 64 bit.
    let aryABI = UpdateUtils.ABI.split("-");
    if (aryABI[0] != "x86" || aryABI[2] != "x64") {
      return false;
    }

    let wrk = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );

    let regPath =
      "SOFTWARE\\Mozilla\\" + Services.appinfo.name + "\\32to64DidMigrate";
    let regValHKCU = WindowsRegistry.readRegKey(
      wrk.ROOT_KEY_CURRENT_USER,
      regPath,
      "Never",
      wrk.WOW64_32
    );
    let regValHKLM = WindowsRegistry.readRegKey(
      wrk.ROOT_KEY_LOCAL_MACHINE,
      regPath,
      "Never",
      wrk.WOW64_32
    );
    // The Never registry key value allows configuring a system to never migrate
    // any of the installations.
    if (regValHKCU === 1 || regValHKLM === 1) {
      LOG("Checker:_getCanMigrate - all installations should not be migrated");
      return false;
    }

    let appBaseDirPath = getAppBaseDir().path;
    regValHKCU = WindowsRegistry.readRegKey(
      wrk.ROOT_KEY_CURRENT_USER,
      regPath,
      appBaseDirPath,
      wrk.WOW64_32
    );
    regValHKLM = WindowsRegistry.readRegKey(
      wrk.ROOT_KEY_LOCAL_MACHINE,
      regPath,
      appBaseDirPath,
      wrk.WOW64_32
    );
    // When the registry value is 1 for the installation directory path value
    // name then the installation has already been migrated once or the system
    // was configured to not migrate that installation.
    if (regValHKCU === 1 || regValHKLM === 1) {
      LOG("Checker:_getCanMigrate - this installation should not be migrated");
      return false;
    }

    // When the registry value is 0 for the installation directory path value
    // name then the installation has updated to Firefox 56 and can be migrated.
    if (regValHKCU === 0 || regValHKLM === 0) {
      LOG("Checker:_getCanMigrate - this installation can be migrated");
      return true;
    }

    LOG("Checker:_getCanMigrate - no registry entries for this installation");
    return false;
  },

  /**
   * The URL of the update service XML file to connect to that contains details
   * about available updates.
   */
  getUpdateURL: async function UC_getUpdateURL(force) {
    this._forced = force;

    let url = Services.appinfo.updateURL;

    if (Services.policies) {
      let policies = Services.policies.getActivePolicies();
      if (policies && "AppUpdateURL" in policies) {
        url = policies.AppUpdateURL.toString();
      }
    }

    if (!url) {
      LOG("Checker:getUpdateURL - update URL not defined");
      return null;
    }

    url = await UpdateUtils.formatUpdateURL(url);

    if (force) {
      url += (url.includes("?") ? "&" : "?") + "force=1";
    }

    if (this._getCanMigrate()) {
      url += (url.includes("?") ? "&" : "?") + "mig64=1";
    }

    LOG("Checker:getUpdateURL - update URL: " + url);
    return url;
  },

  /**
   * See nsIUpdateService.idl
   */
  checkForUpdates: function UC_checkForUpdates(listener, force) {
    LOG("Checker: checkForUpdates, force: " + force);
    gUpdateFileWriteInfo = { phase: "check", failure: false };
    if (!listener) {
      throw Components.Exception("", Cr.NS_ERROR_NULL_POINTER);
    }

    let UpdateServiceInstance = UpdateServiceFactory.createInstance();
    // |force| can override |canCheckForUpdates| since |force| indicates a
    // manual update check. But nothing should override enterprise policies.
    if (UpdateServiceInstance.disabledByPolicy) {
      LOG("Checker: checkForUpdates, disabled by policy");
      return;
    }
    if (!UpdateServiceInstance.canCheckForUpdates && !force) {
      return;
    }

    waitForOtherInstances()
      .then(() => this.getUpdateURL(force))
      .then(url => {
        if (!url) {
          return;
        }

        // It's possible that another check was kicked off and that request sent
        // while we were waiting for other instances to exit here; if the other
        // instances were closed and also the other check was started during the
        // same interval between polls, then here we could now be about to start
        // a second overlapping check, which should not happen. So make sure we
        // don't have a request already active before we start a new one.
        if (this._request) {
          LOG(
            "Checker: checkForUpdates: check request already active, aborting"
          );
          return;
        }

        this._request = new XMLHttpRequest();
        this._request.open("GET", url, true);
        this._request.channel.notificationCallbacks = new CertUtils.BadCertHandler(
          false
        );
        // Prevent the request from reading from the cache.
        this._request.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
        // Prevent the request from writing to the cache.
        this._request.channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
        // Disable cutting edge features, like TLS 1.3, where middleboxes might brick us
        this._request.channel.QueryInterface(
          Ci.nsIHttpChannelInternal
        ).beConservative = true;

        this._request.overrideMimeType("text/xml");
        // The Cache-Control header is only interpreted by proxies and the
        // final destination. It does not help if a resource is already
        // cached locally.
        this._request.setRequestHeader("Cache-Control", "no-cache");
        // HTTP/1.0 servers might not implement Cache-Control and
        // might only implement Pragma: no-cache
        this._request.setRequestHeader("Pragma", "no-cache");

        var self = this;
        this._request.addEventListener("error", function(event) {
          self.onError(event);
        });
        this._request.addEventListener("load", function(event) {
          self.onLoad(event);
        });

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
      throw new Error(
        "Unexpected node name, expected: updates, got: " +
          updatesElement.nodeName
      );
    }

    var updates = [];
    for (var i = 0; i < updatesElement.childNodes.length; ++i) {
      var updateElement = updatesElement.childNodes.item(i);
      if (
        updateElement.nodeType != updateElement.ELEMENT_NODE ||
        updateElement.localName != "update"
      ) {
        continue;
      }

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
    } catch (e) {}

    if (status == 0) {
      status = request.channel.QueryInterface(Ci.nsIRequest).status;
    }
    return status;
  },

  _isHttpStatusCode: function UC__isHttpStatusCode(status) {
    return status >= 100 && status <= 599;
  },

  /**
   * The XMLHttpRequest succeeded and the document was loaded.
   * @param   event
   *          The Event for the load
   */
  onLoad: function UC_onLoad(event) {
    LOG("Checker:onLoad - request completed downloading document");
    Services.prefs.clearUserPref("security.pki.mitm_canary_issuer");
    // Check whether there is a mitm, i.e. check whether the root cert is
    // built-in or not.
    try {
      let sslStatus = request.channel
        .QueryInterface(Ci.nsIRequest)
        .securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
      if (sslStatus && sslStatus.succeededCertChain) {
        let rootCert = null;
        // The root cert is the last cert in the chain.
        if (sslStatus.succeededCertChain.length) {
          rootCert =
            sslStatus.succeededCertChain[
              sslStatus.succeededCertChain.length - 1
            ];
        }
        if (rootCert) {
          Services.prefs.setStringPref(
            "security.pki.mitm_detected",
            !rootCert.isBuiltInRoot
          );
        }
      }
    } catch (e) {
      LOG("Checker:onLoad - Getting sslStatus failed.");
    }

    try {
      // Analyze the resulting DOM and determine the set of updates.
      var updates = this._updates;
      LOG("Checker:onLoad - number of updates available: " + updates.length);

      if (Services.prefs.prefHasUserValue(PREF_APP_UPDATE_BACKGROUNDERRORS)) {
        Services.prefs.clearUserPref(PREF_APP_UPDATE_BACKGROUNDERRORS);
      }

      // Tell the callback about the updates
      this._callback.onCheckComplete(event.target, updates);
    } catch (e) {
      LOG(
        "Checker:onLoad - there was a problem checking for updates. " +
          "Exception: " +
          e
      );
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
   *          The Event for the error
   */
  onError: function UC_onError(event) {
    var request = event.target;
    var status = this._getChannelStatus(request);
    LOG("Checker:onError - request.status: " + status);

    // Set MitM pref.
    try {
      var secInfo = request.channel.securityInfo.QueryInterface(
        Ci.nsITransportSecurityInfo
      );
      if (secInfo.serverCert && secInfo.serverCert.issuerName) {
        Services.prefs.setStringPref(
          "security.pki.mitm_canary_issuer",
          secInfo.serverCert.issuerName
        );
      }
    } catch (e) {
      LOG("Checker:onError - Getting secInfo failed.");
    }

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
   * See nsIUpdateService.idl
   */
  stopCurrentCheck: function UC_stopCurrentCheck() {
    // Always stop the current check
    if (this._request) {
      this._request.abort();
    }
    this._callback = null;
  },

  classID: Components.ID("{898CDC9B-E43F-422F-9CC4-2F6291B415A3}"),
  QueryInterface: ChromeUtils.generateQI(["nsIUpdateChecker"]),
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
   * The nsIRequest object handling the download.
   */
  _request: null,

  /**
   * Whether or not the update being downloaded is a complete replacement of
   * the user's existing installation or a patch representing the difference
   * between the new version and the previous version.
   */
  isCompleteUpdate: null,

  /**
   * We get the nsIRequest from nsIBITS asynchronously. When downloadUpdate has
   * been called, but this._request is not yet valid, _pendingRequest will be
   * a promise that will resolve when this._request has been set.
   */
  _pendingRequest: null,

  /**
   * When using BITS, cancel actions happen asynchronously. This variable
   * keeps track of any cancel action that is in-progress.
   * If the cancel action fails, this will be set back to null so that the
   * action can be attempted again. But if the cancel action succeeds, the
   * resolved promise will remain stored in this variable to prevent cancel
   * from being called twice (which, for BITS, is an error).
   */
  _cancelPromise: null,

  /**
   * BITS receives progress notifications slowly, unless a user is watching.
   * This tracks what frequency notifications are happening at.
   *
   * This is needed because BITS downloads are started asynchronously.
   * Specifically, this is needed to prevent a situation where the download is
   * still starting (Downloader._pendingRequest has not resolved) when the first
   * observer registers itself. Without this variable, there is no way of
   * knowing whether the download was started as Active or Idle and, therefore,
   * we don't know if we need to start Active mode when _pendingRequest
   * resolves.
   */
  _bitsActiveNotifications: false,

  /**
   * This is a function that when called will stop the update process from
   * waiting for language pack updates. This is for safety to ensure that a
   * problem in the add-ons manager doesn't delay updates by much.
   */
  _langPackTimeout: null,

  /**
   * Cancels the active download.
   *
   * For a BITS download, this will cancel and remove the download job. For
   * an nsIIncrementalDownload, this will stop the download, but leaves the
   * data around to allow the transfer to be resumed later.
   */
  cancel: async function Downloader_cancel(cancelError) {
    LOG("Downloader: cancel");
    if (cancelError === undefined) {
      cancelError = Cr.NS_BINDING_ABORTED;
    }
    if (this.usingBits) {
      // If a cancel action is already in progress, just return when that
      // promise resolved. Trying to cancel the same request twice is an error.
      if (this._cancelPromise) {
        await this._cancelPromise;
        return;
      }

      if (this._pendingRequest) {
        await this._pendingRequest;
      }
      if (this._patch.getProperty("bitsId") != null) {
        // Make sure that we don't try to resume this download after it was
        // cancelled.
        this._patch.deleteProperty("bitsId");
      }
      try {
        this._cancelPromise = this._request.cancelAsync(cancelError);
        await this._cancelPromise;
      } catch (e) {
        // On success, we will not set the cancel promise to null because
        // we want to prevent two cancellations of the same request. But
        // retrying after a failed cancel is not an error, so we will set the
        // cancel promise to null in the failure case.
        this._cancelPromise = null;
        throw e;
      }
    } else if (this._request && this._request instanceof Ci.nsIRequest) {
      this._request.cancel(cancelError);
    }
  },

  /**
   * Verify the downloaded file.  We assume that the download is complete at
   * this point.
   */
  _verifyDownload: function Downloader__verifyDownload() {
    LOG("Downloader:_verifyDownload called");
    if (!this._request) {
      AUSTLMY.pingDownloadCode(
        this.isCompleteUpdate,
        AUSTLMY.DWNLD_ERR_VERIFY_NO_REQUEST
      );
      return false;
    }

    let destination = getDownloadingUpdateDir();
    destination.append(FILE_UPDATE_MAR);

    // Ensure that the file size matches the expected file size.
    if (destination.fileSize != this._patch.size) {
      LOG("Downloader:_verifyDownload downloaded size != expected size.");
      AUSTLMY.pingDownloadCode(
        this.isCompleteUpdate,
        AUSTLMY.DWNLD_ERR_VERIFY_PATCH_SIZE_NOT_EQUAL
      );
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

    // Look to see if any of the patches in the Update object has been
    // pre-selected for download, otherwise we must figure out which one
    // to select ourselves.
    var selectedPatch = update.selectedPatch;

    var state = selectedPatch ? selectedPatch.state : STATE_NONE;

    // If this is a patch that we know about, then select it.  If it is a patch
    // that we do not know about, then remove it and use our default logic.
    var useComplete = false;
    if (selectedPatch) {
      LOG(
        "Downloader:_selectPatch - found existing patch with state: " + state
      );
      if (state == STATE_DOWNLOADING) {
        LOG("Downloader:_selectPatch - resuming download");
        return selectedPatch;
      }
      if (
        state == STATE_PENDING ||
        state == STATE_PENDING_SERVICE ||
        state == STATE_PENDING_ELEVATE ||
        state == STATE_APPLIED ||
        state == STATE_APPLIED_SERVICE
      ) {
        LOG("Downloader:_selectPatch - already downloaded");
        return null;
      }

      // When downloading the patch failed using BITS, there hasn't been an
      // attempt to download the patch using the internal application download
      // mechanism, and an attempt to stage or apply the patch hasn't failed
      // which indicates that a different patch should be downloaded since
      // re-downloading the same patch with the internal application download
      // mechanism will likely also fail when trying to stage or apply it then
      // try to download the same patch using the internal application download
      // mechanism.
      selectedPatch.QueryInterface(Ci.nsIWritablePropertyBag);
      if (
        selectedPatch.getProperty("bitsResult") != null &&
        selectedPatch.getProperty("internalResult") == null &&
        !selectedPatch.errorCode
      ) {
        LOG(
          "Downloader:_selectPatch - Falling back to non-BITS download " +
            "mechanism for the same patch due to existing BITS result: " +
            selectedPatch.getProperty("bitsResult")
        );
        return selectedPatch;
      }

      if (update && selectedPatch.type == "complete") {
        // This is a pretty fatal error.  Just bail.
        LOG("Downloader:_selectPatch - failed to apply complete patch!");
        cleanupDownloadingUpdate();
        return null;
      }

      // Something went wrong when we tried to apply the previous patch.
      // Try the complete patch next time.
      useComplete = true;
      selectedPatch = null;
    }

    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );

    // If we were not able to discover an update from a previous download, we
    // select the best patch from the given set.
    var partialPatch = getPatchOfType(update, "partial");
    if (!useComplete) {
      selectedPatch = partialPatch;
    }
    if (!selectedPatch) {
      if (um.readyUpdate) {
        // If we already have a ready update, we download partials only.
        LOG(
          "Downloader:_selectPatch - not selecting a complete patch because " +
            "this is not the first download of the session"
        );
        return null;
      }

      if (partialPatch) {
        partialPatch.selected = false;
      }
      selectedPatch = getPatchOfType(update, "complete");
    }

    // if update only contains a partial patch, selectedPatch == null here if
    // the partial patch has been attempted and fails and we're trying to get a
    // complete patch
    if (selectedPatch) {
      selectedPatch.selected = true;
      update.isCompleteUpdate = selectedPatch.type == "complete";
    }

    um.downloadingUpdate = update;

    return selectedPatch;
  },

  /**
   * Whether or not the user wants to be notified that an update is being
   * downloaded.
   */
  get _notifyDuringDownload() {
    return Services.prefs.getBoolPref(
      PREF_APP_UPDATE_NOTIFYDURINGDOWNLOAD,
      false
    );
  },

  _notifyDownloadStatusObservers: function Downloader_notifyDownloadStatusObservers() {
    if (this._notifyDuringDownload) {
      let status = this.updateService.isDownloading ? "downloading" : "idle";
      Services.obs.notifyObservers(this._update, "update-downloading", status);
    }
  },

  /**
   * Whether or not we are currently downloading something.
   */
  get isBusy() {
    return this._request != null || this._pendingRequest != null;
  },

  get usingBits() {
    return this._pendingRequest != null || this._request instanceof BitsRequest;
  },

  /**
   * Returns true if the specified patch can be downloaded with BITS.
   */
  _canUseBits: function Downloader__canUseBits(patch) {
    if (getCanUseBits() != "CanUseBits") {
      // This will have printed its own logging. No need to print more.
      return false;
    }
    // Regardless of success or failure, don't download the same patch with BITS
    // twice.
    if (patch.getProperty("bitsResult") != null) {
      LOG(
        "Downloader:_canUseBits - Not using BITS because it was already tried"
      );
      return false;
    }
    LOG("Downloader:_canUseBits - Patch is able to use BITS download");
    return true;
  },

  /**
   * Instruct the add-ons manager to start downloading language pack updates in
   * preparation for the current update.
   */
  _startLangPackUpdates: function Downloader__startLangPackUpdates() {
    if (!Services.prefs.getBoolPref(PREF_APP_UPDATE_LANGPACK_ENABLED, false)) {
      return;
    }

    // A promise that we can resolve at some point to time out the language pack
    // update process.
    let timeoutPromise = new Promise(resolve => {
      this._langPackTimeout = resolve;
    });

    let update = unwrap(this._update);

    let existing = LangPackUpdates.get(update);
    if (existing) {
      // We have already started staging lang packs for this update, no need to
      // do it again.
      return;
    }

    // Note that we don't care about success or failure here, either way we will
    // continue with the update process.
    let langPackPromise = AddonManager.stageLangpacksForAppUpdate(
      update.appVersion,
      update.appVersion
    )
      .catch(error => {
        LOG(
          `Add-ons manager threw exception while updating language packs: ${error}`
        );
      })
      .finally(() => {
        this._langPackTimeout = null;

        if (TelemetryStopwatch.running("UPDATE_LANGPACK_OVERTIME", update)) {
          TelemetryStopwatch.finish("UPDATE_LANGPACK_OVERTIME", update);
        }
      });

    LangPackUpdates.set(
      update,
      Promise.race([langPackPromise, timeoutPromise])
    );
  },

  /**
   * Download and stage the given update.
   * @param   update
   *          A nsIUpdate object to download a patch for. Cannot be null.
   */
  downloadUpdate: function Downloader_downloadUpdate(update) {
    LOG("UpdateService:_downloadUpdate");
    gUpdateFileWriteInfo = { phase: "download", failure: false };
    if (!update) {
      AUSTLMY.pingDownloadCode(undefined, AUSTLMY.DWNLD_ERR_NO_UPDATE);
      throw Components.Exception("", Cr.NS_ERROR_NULL_POINTER);
    }

    var updateDir = getDownloadingUpdateDir();

    this._update = update;

    // This function may return null, which indicates that there are no patches
    // to download.
    this._patch = this._selectPatch(update, updateDir);
    if (!this._patch) {
      LOG("Downloader:downloadUpdate - no patch to download");
      AUSTLMY.pingDownloadCode(undefined, AUSTLMY.DWNLD_ERR_NO_UPDATE_PATCH);
      return false;
    }
    // The update and the patch implement nsIWritablePropertyBag. Expose that
    // interface immediately after a patch is assigned so that
    // this.(_patch|_update).(get|set)Property can always safely be called.
    this._update.QueryInterface(Ci.nsIWritablePropertyBag);
    this._patch.QueryInterface(Ci.nsIWritablePropertyBag);

    this.isCompleteUpdate = this._patch.type == "complete";

    let canUseBits = false;
    // Allow the advertised update to disable BITS.
    if (this._update.getProperty("disableBITS") != null) {
      LOG(
        "Downloader:downloadUpdate - BITS downloads disabled by update " +
          "advertisement"
      );
    } else {
      canUseBits = this._canUseBits(this._patch);
    }

    if (!canUseBits) {
      let patchFile = updateDir.clone();
      patchFile.append(FILE_UPDATE_MAR);

      // The interval is 0 since there is no need to throttle downloads.
      let interval = 0;

      LOG(
        "Downloader:downloadUpdate - Starting nsIIncrementalDownload with " +
          "url: " +
          this._patch.URL +
          ", path: " +
          patchFile.path +
          ", interval: " +
          interval
      );
      let uri = Services.io.newURI(this._patch.URL);

      this._request = Cc[
        "@mozilla.org/network/incremental-download;1"
      ].createInstance(Ci.nsIIncrementalDownload);
      this._request.init(uri, patchFile, DOWNLOAD_CHUNK_SIZE, interval);
      this._request.start(this, null);
    } else {
      let noProgressTimeout = BITS_IDLE_NO_PROGRESS_TIMEOUT_SECS;
      let monitorInterval = BITS_IDLE_POLL_RATE_MS;
      this._bitsActiveNotifications = false;
      // The monitor's timeout should be much greater than the longest monitor
      // poll interval. If the timeout is too short, delay in the pipe to the
      // update agent might cause BITS to falsely report an error, causing an
      // unnecessary fallback to nsIIncrementalDownload.
      let monitorTimeout = Math.max(10 * monitorInterval, 10 * 60 * 1000);
      if (this.hasDownloadListeners) {
        noProgressTimeout = BITS_ACTIVE_NO_PROGRESS_TIMEOUT_SECS;
        monitorInterval = BITS_ACTIVE_POLL_RATE_MS;
        this._bitsActiveNotifications = true;
      }

      let updateRootDir = FileUtils.getDir(KEY_UPDROOT, [], true);
      let jobName = "MozillaUpdate " + updateRootDir.leafName;
      let updatePath = updateDir.path;
      if (!Bits.initialized) {
        Bits.init(jobName, updatePath, monitorTimeout);
      }

      this._cancelPromise = null;

      let bitsId = this._patch.getProperty("bitsId");
      if (bitsId) {
        LOG(
          "Downloader:downloadUpdate - Connecting to in-progress download. " +
            "BITS ID: " +
            bitsId
        );

        this._pendingRequest = Bits.monitorDownload(
          bitsId,
          monitorInterval,
          this,
          null
        );
      } else {
        LOG(
          "Downloader:downloadUpdate - Starting BITS download with url: " +
            this._patch.URL +
            ", updateDir: " +
            updatePath +
            ", filename: " +
            FILE_UPDATE_MAR
        );

        this._pendingRequest = Bits.startDownload(
          this._patch.URL,
          FILE_UPDATE_MAR,
          Ci.nsIBits.PROXY_PRECONFIG,
          noProgressTimeout,
          monitorInterval,
          this,
          null
        );
      }
      this._pendingRequest = this._pendingRequest.then(
        request => {
          this._request = request;
          this._patch.setProperty("bitsId", request.bitsId);

          LOG(
            "Downloader:downloadUpdate - BITS download running. BITS ID: " +
              request.bitsId
          );

          if (this.hasDownloadListeners) {
            this._maybeStartActiveNotifications();
          } else {
            this._maybeStopActiveNotifications();
          }

          Cc["@mozilla.org/updates/update-manager;1"]
            .getService(Ci.nsIUpdateManager)
            .saveUpdates();
          this._pendingRequest = null;
        },
        error => {
          if (
            (error.type == Ci.nsIBits.ERROR_TYPE_FAILED_TO_GET_BITS_JOB ||
              error.type == Ci.nsIBits.ERROR_TYPE_FAILED_TO_CONNECT_TO_BCM) &&
            error.action == Ci.nsIBits.ERROR_ACTION_MONITOR_DOWNLOAD &&
            error.stage == Ci.nsIBits.ERROR_STAGE_BITS_CLIENT &&
            error.codeType == Ci.nsIBits.ERROR_CODE_TYPE_HRESULT &&
            error.code == HRESULT_E_ACCESSDENIED
          ) {
            LOG(
              "Downloader:downloadUpdate - Failed to connect to existing " +
                "BITS job. It is likely owned by another user."
            );
            // This isn't really a failure code since the BITS job may be working
            // just fine on another account, so convert this to a code that
            // indicates that. This will make it easier to identify in telemetry.
            error.type = Ci.nsIBits.ERROR_TYPE_ACCESS_DENIED_EXPECTED;
            error.codeType = Ci.nsIBits.ERROR_CODE_TYPE_NONE;
            error.code = null;
            // When we detect this situation, disable BITS until Firefox shuts
            // down. There are a couple of reasons for this. First, without any
            // kind of flag, we enter an infinite loop here where we keep trying
            // BITS over and over again (normally setting bitsResult prevents
            // this, but we don't know the result of the BITS job, so we don't
            // want to set that). Second, since we are trying to update, this
            // process must have the update mutex. We don't ever give up the
            // update mutex, so even if the other user starts Firefox, they will
            // not complete the BITS job while this Firefox instance is around.
            gBITSInUseByAnotherUser = true;
          } else {
            this._patch.setProperty("bitsResult", Cr.NS_ERROR_FAILURE);
            Cc["@mozilla.org/updates/update-manager;1"]
              .getService(Ci.nsIUpdateManager)
              .saveUpdates();

            LOG(
              "Downloader:downloadUpdate - Failed to start to BITS job. " +
                "Error: " +
                error
            );
          }

          this._pendingRequest = null;

          AUSTLMY.pingBitsError(this.isCompleteUpdate, error);

          // Try download again with nsIIncrementalDownload
          // The update status file has already had STATE_DOWNLOADING written to
          // it. If the downloadUpdate call below returns early, that status
          // should probably be rewritten. However, the only conditions that might
          // cause it to return early would have prevented this code from running.
          // So it should be fine.
          this.downloadUpdate(this._update);
        }
      );
    }

    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (!um.readyUpdate) {
      writeStatusFile(getReadyUpdateDir(), STATE_DOWNLOADING);
    }
    if (this._patch.state != STATE_DOWNLOADING) {
      this._patch.state = STATE_DOWNLOADING;
      Cc["@mozilla.org/updates/update-manager;1"]
        .getService(Ci.nsIUpdateManager)
        .saveUpdates();
    }

    this._startLangPackUpdates();

    this._notifyDownloadStatusObservers();

    return true;
  },

  /**
   * This is run when a download listener is added.
   */
  onDownloadListenerAdded: function Downloader_onDownloadListenerAdded() {
    // Increase the status update frequency when someone starts listening
    this._maybeStartActiveNotifications();
  },

  /**
   * This is run when a download listener is removed.
   */
  onDownloadListenerRemoved: function Downloader_onDownloadListenerRemoved() {
    // Decrease the status update frequency when no one is listening
    if (!this.hasDownloadListeners) {
      this._maybeStopActiveNotifications();
    }
  },

  get hasDownloadListeners() {
    return this.updateService.hasDownloadListeners;
  },

  /**
   * This speeds up BITS progress notifications in response to a user watching
   * the notifications.
   */
  _maybeStartActiveNotifications: async function Downloader__maybeStartActiveNotifications() {
    if (
      this.usingBits &&
      !this._bitsActiveNotifications &&
      this.hasDownloadListeners &&
      this._request
    ) {
      LOG(
        "Downloader:_maybeStartActiveNotifications - Starting active " +
          "notifications"
      );
      this._bitsActiveNotifications = true;
      await Promise.all([
        this._request
          .setNoProgressTimeout(BITS_ACTIVE_NO_PROGRESS_TIMEOUT_SECS)
          .catch(error => {
            LOG(
              "Downloader:_maybeStartActiveNotifications - Failed to set " +
                "no progress timeout. Error: " +
                error
            );
          }),
        this._request
          .changeMonitorInterval(BITS_ACTIVE_POLL_RATE_MS)
          .catch(error => {
            LOG(
              "Downloader:_maybeStartActiveNotifications - Failed to increase " +
                "status update frequency. Error: " +
                error
            );
          }),
      ]);
    }
  },

  /**
   * This slows down BITS progress notifications in response to a user no longer
   * watching the notifications.
   */
  _maybeStopActiveNotifications: async function Downloader__maybeStopActiveNotifications() {
    if (
      this.usingBits &&
      this._bitsActiveNotifications &&
      !this.hasDownloadListeners &&
      this._request
    ) {
      LOG(
        "Downloader:_maybeStopActiveNotifications - Stopping active " +
          "notifications"
      );
      this._bitsActiveNotifications = false;
      await Promise.all([
        this._request
          .setNoProgressTimeout(BITS_IDLE_NO_PROGRESS_TIMEOUT_SECS)
          .catch(error => {
            LOG(
              "Downloader:_maybeStopActiveNotifications - Failed to set " +
                "no progress timeout: " +
                error
            );
          }),
        this._request
          .changeMonitorInterval(BITS_IDLE_POLL_RATE_MS)
          .catch(error => {
            LOG(
              "Downloader:_maybeStopActiveNotifications - Failed to decrease " +
                "status update frequency: " +
                error
            );
          }),
      ]);
    }
  },

  /**
   * When the async request begins
   * @param   request
   *          The nsIRequest object for the transfer
   */
  onStartRequest: function Downloader_onStartRequest(request) {
    if (!this.usingBits) {
      LOG(
        "Downloader:onStartRequest - original URI spec: " +
          request.URI.spec +
          ", final URI spec: " +
          request.finalURI.spec
      );
      // Set finalURL in onStartRequest if it is different.
      if (this._patch.finalURL != request.finalURI.spec) {
        this._patch.finalURL = request.finalURI.spec;
        Cc["@mozilla.org/updates/update-manager;1"]
          .getService(Ci.nsIUpdateManager)
          .saveUpdates();
      }
    }

    this.updateService.forEachDownloadListener(listener => {
      listener.onStartRequest(request);
    });
  },

  /**
   * When new data has been downloaded
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   progress
   *          The current number of bytes transferred
   * @param   maxProgress
   *          The total number of bytes that must be transferred
   */
  onProgress: function Downloader_onProgress(request, progress, maxProgress) {
    LOG("Downloader:onProgress - progress: " + progress + "/" + maxProgress);

    if (progress > this._patch.size) {
      LOG(
        "Downloader:onProgress - progress: " +
          progress +
          " is higher than patch size: " +
          this._patch.size
      );
      AUSTLMY.pingDownloadCode(
        this.isCompleteUpdate,
        AUSTLMY.DWNLD_ERR_PATCH_SIZE_LARGER
      );
      this.cancel(Cr.NS_ERROR_UNEXPECTED);
      return;
    }

    // Wait until the transfer has started (progress > 0) to verify maxProgress
    // so that we don't check it before it is available (in which case, -1 would
    // have been passed).
    if (progress > 0 && maxProgress != this._patch.size) {
      LOG(
        "Downloader:onProgress - maxProgress: " +
          maxProgress +
          " is not equal to expected patch size: " +
          this._patch.size
      );
      AUSTLMY.pingDownloadCode(
        this.isCompleteUpdate,
        AUSTLMY.DWNLD_ERR_PATCH_SIZE_NOT_EQUAL
      );
      this.cancel(Cr.NS_ERROR_UNEXPECTED);
      return;
    }

    this.updateService.forEachDownloadListener(listener => {
      if (listener instanceof Ci.nsIProgressEventSink) {
        listener.onProgress(request, progress, maxProgress);
      }
    });
    this.updateService._consecutiveSocketErrors = 0;
  },

  /**
   * When we have new status text
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   status
   *          A status code
   * @param   statusText
   *          Human readable version of |status|
   */
  onStatus: function Downloader_onStatus(request, status, statusText) {
    LOG(
      "Downloader:onStatus - status: " + status + ", statusText: " + statusText
    );

    this.updateService.forEachDownloadListener(listener => {
      if (listener instanceof Ci.nsIProgressEventSink) {
        listener.onStatus(request, status, statusText);
      }
    });
  },

  /**
   * When data transfer ceases
   * @param   request
   *          The nsIRequest object for the transfer
   * @param   status
   *          Status code containing the reason for the cessation.
   */
  /* eslint-disable-next-line complexity */
  onStopRequest: async function Downloader_onStopRequest(request, status) {
    if (!this.usingBits) {
      LOG(
        "Downloader:onStopRequest - downloader: nsIIncrementalDownload, " +
          "original URI spec: " +
          request.URI.spec +
          ", final URI spec: " +
          request.finalURI.spec +
          ", status: " +
          status
      );
    } else {
      LOG("Downloader:onStopRequest - downloader: BITS, status: " + status);
    }

    let bitsCompletionError;
    if (this.usingBits) {
      if (Components.isSuccessCode(status)) {
        try {
          await request.complete();
        } catch (e) {
          LOG(
            "Downloader:onStopRequest - Unable to complete BITS download: " + e
          );
          status = Cr.NS_ERROR_FAILURE;
          bitsCompletionError = e;
        }
      } else {
        // BITS jobs that failed to complete should still have cancel called on
        // them to remove the job.
        try {
          await this.cancel();
        } catch (e) {
          // This will fail if the job stopped because it was cancelled.
          // Even if this is a "real" error, there isn't really anything to do
          // about it, and it's not really a big problem. It just means that the
          // BITS job will stay around until it is removed automatically
          // (default of 90 days).
        }
      }
    }

    var state = this._patch.state;
    var shouldShowPrompt = false;
    var shouldRegisterOnlineObserver = false;
    var shouldRetrySoon = false;
    var deleteActiveUpdate = false;
    let migratedToReadyUpdate = false;
    let nonDownloadFailure = false;
    var retryTimeout = Services.prefs.getIntPref(
      PREF_APP_UPDATE_SOCKET_RETRYTIMEOUT,
      DEFAULT_SOCKET_RETRYTIMEOUT
    );
    // Prevent the preference from setting a value greater than 10000.
    retryTimeout = Math.min(retryTimeout, 10000);
    var maxFail = Services.prefs.getIntPref(
      PREF_APP_UPDATE_SOCKET_MAXERRORS,
      DEFAULT_SOCKET_MAX_ERRORS
    );
    // Prevent the preference from setting a value greater than 20.
    maxFail = Math.min(maxFail, 20);
    let permissionFixingInProgress = false;
    LOG(
      "Downloader:onStopRequest - status: " +
        status +
        ", " +
        "current fail: " +
        this.updateService._consecutiveSocketErrors +
        ", " +
        "max fail: " +
        maxFail +
        ", " +
        "retryTimeout: " +
        retryTimeout
    );
    if (Components.isSuccessCode(status)) {
      if (this._verifyDownload()) {
        AUSTLMY.pingDownloadCode(this.isCompleteUpdate, AUSTLMY.DWNLD_SUCCESS);

        // We're about to clobber the ready update so we can replace it with the
        // downloading update that just finished. We need to let observers know
        // about this.
        Services.obs.notifyObservers(this._update, "update-swap");

        // Swap the downloading update into the ready update directory.
        cleanUpReadyUpdateDir();
        let downloadedMar = getDownloadingUpdateDir();
        downloadedMar.append(FILE_UPDATE_MAR);
        let readyDir = getReadyUpdateDir();
        try {
          downloadedMar.moveTo(readyDir, FILE_UPDATE_MAR);
          migratedToReadyUpdate = true;
        } catch (e) {
          migratedToReadyUpdate = false;
        }

        if (migratedToReadyUpdate) {
          AUSTLMY.pingMoveResult(AUSTLMY.MOVE_RESULT_SUCCESS);
          state = getBestPendingState();
          shouldShowPrompt = !getCanStageUpdates();

          // Tell the updater.exe we're ready to apply.
          writeStatusFile(getReadyUpdateDir(), state);
          writeVersionFile(getReadyUpdateDir(), this._update.appVersion);
          this._update.installDate = new Date().getTime();
          this._update.statusText = gUpdateBundle.GetStringFromName(
            "installPending"
          );
          Services.prefs.setIntPref(PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS, 0);
        } else {
          LOG(
            "Downloader:onStopRequest - failed to move the downloading " +
              "update to the ready update directory."
          );
          AUSTLMY.pingMoveResult(AUSTLMY.MOVE_RESULT_UNKNOWN_FAILURE);

          state = STATE_DOWNLOAD_FAILED;
          status = Cr.NS_ERROR_FILE_COPY_OR_MOVE_FAILED;

          const mfCode = "move_failed";
          let message = getStatusTextFromCode(mfCode, mfCode);
          this._update.statusText = message;

          nonDownloadFailure = true;
          deleteActiveUpdate = true;

          cleanUpDownloadingUpdateDir();

          let failedWrite = readyDir.clone();
          failedWrite.append(FILE_UPDATE_MAR);
          permissionFixingInProgress = handleCriticalWriteFailure(
            failedWrite.path
          );
        }
      } else {
        LOG("Downloader:onStopRequest - download verification failed");
        state = STATE_DOWNLOAD_FAILED;
        status = Cr.NS_ERROR_CORRUPTED_CONTENT;

        // Yes, this code is a string.
        const vfCode = "verification_failed";
        var message = getStatusTextFromCode(vfCode, vfCode);
        this._update.statusText = message;

        if (this._update.isCompleteUpdate || this._update.patchCount != 2) {
          deleteActiveUpdate = true;
        }

        // Destroy the updates directory, since we're done with it.
        cleanUpDownloadingUpdateDir();
      }
    } else if (status == Cr.NS_ERROR_OFFLINE) {
      // Register an online observer to try again.
      // The online observer will continue the incremental download by
      // calling downloadUpdate on the active update which continues
      // downloading the file from where it was.
      LOG("Downloader:onStopRequest - offline, register online observer: true");
      AUSTLMY.pingDownloadCode(
        this.isCompleteUpdate,
        AUSTLMY.DWNLD_RETRY_OFFLINE
      );
      shouldRegisterOnlineObserver = true;
      deleteActiveUpdate = false;

      // Each of NS_ERROR_NET_TIMEOUT, ERROR_CONNECTION_REFUSED,
      // NS_ERROR_NET_RESET and NS_ERROR_DOCUMENT_NOT_CACHED can be returned
      // when disconnecting the internet while a download of a MAR is in
      // progress.  There may be others but I have not encountered them during
      // testing.
    } else if (
      (status == Cr.NS_ERROR_NET_TIMEOUT ||
        status == Cr.NS_ERROR_CONNECTION_REFUSED ||
        status == Cr.NS_ERROR_NET_RESET ||
        status == Cr.NS_ERROR_DOCUMENT_NOT_CACHED) &&
      this.updateService._consecutiveSocketErrors < maxFail
    ) {
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
    } else if (status != Cr.NS_BINDING_ABORTED && status != Cr.NS_ERROR_ABORT) {
      if (
        status == Cr.NS_ERROR_FILE_ACCESS_DENIED ||
        status == Cr.NS_ERROR_FILE_READ_ONLY
      ) {
        LOG("Downloader:onStopRequest - permission error");
        // This will either fix the permissions, or asynchronously show the
        // reinstall prompt if it cannot fix them.
        let patchFile = getDownloadingUpdateDir();
        patchFile.append(FILE_UPDATE_MAR);
        permissionFixingInProgress = handleCriticalWriteFailure(patchFile.path);
        nonDownloadFailure = true;
      } else {
        LOG("Downloader:onStopRequest - non-verification failure");
      }

      let dwnldCode = AUSTLMY.DWNLD_ERR_BINDING_ABORTED;
      if (status == Cr.NS_ERROR_ABORT) {
        dwnldCode = AUSTLMY.DWNLD_ERR_ABORT;
      }
      AUSTLMY.pingDownloadCode(this.isCompleteUpdate, dwnldCode);

      // Some sort of other failure, log this in the |statusText| property
      state = STATE_DOWNLOAD_FAILED;

      // XXXben - if |request| (The Incremental Download) provided a means
      // for accessing the http channel we could do more here.

      this._update.statusText = getStatusTextFromCode(
        status,
        Cr.NS_BINDING_FAILED
      );

      // Destroy the updates directory, since we're done with it.
      cleanUpDownloadingUpdateDir();

      deleteActiveUpdate = true;
    }
    if (!this.usingBits) {
      this._patch.setProperty("internalResult", status);
    } else {
      this._patch.setProperty("bitsResult", status);

      // If we failed when using BITS, we want to override the retry decision
      // since we need to retry with nsIncrementalDownload before we give up.
      // However, if the download was cancelled, don't retry. If the transfer
      // was cancelled, we don't want it to restart on its own.
      if (
        !Components.isSuccessCode(status) &&
        status != Cr.NS_BINDING_ABORTED &&
        status != Cr.NS_ERROR_ABORT
      ) {
        deleteActiveUpdate = false;
        shouldRetrySoon = true;
      }

      // Send BITS Telemetry
      if (Components.isSuccessCode(status)) {
        AUSTLMY.pingBitsSuccess(this.isCompleteUpdate);
      } else {
        let error;
        if (bitsCompletionError) {
          error = bitsCompletionError;
        } else if (status == Cr.NS_ERROR_CORRUPTED_CONTENT) {
          error = new BitsVerificationError();
        } else {
          error = request.transferError;
          if (!error) {
            error = new BitsUnknownError();
          }
        }
        AUSTLMY.pingBitsError(this.isCompleteUpdate, error);
      }
    }

    LOG("Downloader:onStopRequest - setting state to: " + state);
    if (this._patch.state != state) {
      this._patch.state = state;
    }
    var um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (deleteActiveUpdate) {
      this._update.installDate = new Date().getTime();
      um.addUpdateToHistory(um.downloadingUpdate);
      um.downloadingUpdate = null;
    } else if (um.downloadingUpdate && um.downloadingUpdate.state != state) {
      um.downloadingUpdate.state = state;
    }
    if (migratedToReadyUpdate) {
      um.readyUpdate = um.downloadingUpdate;
      um.downloadingUpdate = null;
    }
    um.saveUpdates();

    // Only notify listeners about the stopped state if we
    // aren't handling an internal retry.
    if (!shouldRetrySoon && !shouldRegisterOnlineObserver) {
      this.updateService.forEachDownloadListener(listener => {
        listener.onStopRequest(request, status);
      });
    }

    this._request = null;

    // This notification must happen after _request is set to null so that
    // the correct this.updateService.isDownloading value is available in
    // _notifyDownloadStatusObservers().
    this._notifyDownloadStatusObservers();

    if (state == STATE_DOWNLOAD_FAILED) {
      var allFailed = true;
      // Don't bother retrying the download if we got an error that isn't
      // download related.
      if (!nonDownloadFailure) {
        // If we haven't already, attempt to download without BITS
        if (request instanceof BitsRequest) {
          LOG(
            "Downloader:onStopRequest - BITS download failed. Falling back " +
              "to nsIIncrementalDownload"
          );
          let success = this.downloadUpdate(this._update);
          if (!success) {
            cleanupDownloadingUpdate();
          } else {
            allFailed = false;
          }
        }

        // Check if there is a complete update patch that can be downloaded.
        if (
          allFailed &&
          !this._update.isCompleteUpdate &&
          this._update.patchCount == 2
        ) {
          LOG(
            "Downloader:onStopRequest - verification of patch failed, " +
              "downloading complete update patch"
          );
          this._update.isCompleteUpdate = true;
          let success = this.downloadUpdate(this._update);

          if (!success) {
            cleanupDownloadingUpdate();
          } else {
            allFailed = false;
          }
        }
      }

      if (allFailed && !permissionFixingInProgress) {
        let downloadAttempts = Services.prefs.getIntPref(
          PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS,
          0
        );
        downloadAttempts++;
        Services.prefs.setIntPref(
          PREF_APP_UPDATE_DOWNLOAD_ATTEMPTS,
          downloadAttempts
        );
        let maxAttempts = Math.min(
          Services.prefs.getIntPref(PREF_APP_UPDATE_DOWNLOAD_MAXATTEMPTS, 2),
          10
        );

        if (downloadAttempts > maxAttempts) {
          LOG(
            "Downloader:onStopRequest - notifying observers of error. " +
              "topic: update-error, status: download-attempts-exceeded, " +
              "downloadAttempts: " +
              downloadAttempts +
              " " +
              "maxAttempts: " +
              maxAttempts
          );
          Services.obs.notifyObservers(
            this._update,
            "update-error",
            "download-attempts-exceeded"
          );
        } else {
          this._update.selectedPatch.selected = false;
          LOG(
            "Downloader:onStopRequest - notifying observers of error. " +
              "topic: update-error, status: download-attempt-failed"
          );
          Services.obs.notifyObservers(
            this._update,
            "update-error",
            "download-attempt-failed"
          );
        }
      }
      if (allFailed) {
        // We don't care about language pack updates now.
        this._langPackTimeout = null;
        LangPackUpdates.delete(unwrap(this._update));

        // Prevent leaking the update object (bug 454964).
        this._update = null;

        // allFailed indicates that we didn't (successfully) call downloadUpdate
        // to try to download a different MAR. In this case, this Downloader
        // is no longer being used.
        this.updateService._downloader = null;
      }
      // A complete download has been initiated or the failure was handled.
      return;
    }

    // If the download has succeeded or failed, we are done with this Downloader
    // object. However, in some cases (ex: network disconnection), we will
    // attempt to resume using this same Downloader.
    if (state != STATE_DOWNLOADING) {
      this.updateService._downloader = null;
    }

    if (
      state == STATE_PENDING ||
      state == STATE_PENDING_SERVICE ||
      state == STATE_PENDING_ELEVATE
    ) {
      if (getCanStageUpdates()) {
        LOG(
          "Downloader:onStopRequest - attempting to stage update: " +
            this._update.name
        );
        gUpdateFileWriteInfo = { phase: "stage", failure: false };
        // Stage the update
        try {
          Cc["@mozilla.org/updates/update-processor;1"]
            .createInstance(Ci.nsIUpdateProcessor)
            .processUpdate();
          gStagingInProgress = true;
        } catch (e) {
          // Fail gracefully in case the application does not support the update
          // processor service.
          LOG(
            "Downloader:onStopRequest - failed to stage update. Exception: " + e
          );
          shouldShowPrompt = true;
        }
      }
    }

    // If we're still waiting on language pack updates then run a timer to time
    // out the attempt after an appropriate amount of time.
    if (this._langPackTimeout) {
      // Start a timer to measure how much longer it takes for the language
      // packs to stage.
      TelemetryStopwatch.start(
        "UPDATE_LANGPACK_OVERTIME",
        unwrap(this._update),
        { inSeconds: true }
      );

      setTimeout(
        this._langPackTimeout,
        Services.prefs.getIntPref(
          PREF_APP_UPDATE_LANGPACK_TIMEOUT,
          LANGPACK_UPDATE_DEFAULT_TIMEOUT
        )
      );
    }

    // Do this after *everything* else, since it will likely cause the app
    // to shut down.
    if (shouldShowPrompt) {
      // Wait for language packs to stage before showing any prompt to restart.
      let update = this._update;
      promiseLangPacksUpdated(update).then(() => {
        LOG(
          "Downloader:onStopRequest - Notifying observers that " +
            "an update was downloaded. topic: update-downloaded, status: " +
            update.state
        );
        Services.obs.notifyObservers(update, "update-downloaded", update.state);
      });
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
      this.updateService._retryTimer = Cc[
        "@mozilla.org/timer;1"
      ].createInstance(Ci.nsITimer);
      this.updateService._retryTimer.initWithCallback(
        function() {
          this._attemptResume();
        }.bind(this.updateService),
        retryTimeout,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    } else {
      // Prevent leaking the update object (bug 454964)
      this._update = null;
    }
  },

  /**
   * This function should be called when shutting down so that resources get
   * freed properly.
   */
  cleanup: function Downloader_cleanup() {
    if (this.usingBits) {
      if (this._pendingRequest) {
        this._pendingRequest.then(() => this._request.shutdown());
      } else {
        this._request.shutdown();
      }
    }
  },

  /**
   * See nsIInterfaceRequestor.idl
   */
  getInterface: function Downloader_getInterface(iid) {
    // The network request may require proxy authentication, so provide the
    // default nsIAuthPrompt if requested.
    if (iid.equals(Ci.nsIAuthPrompt)) {
      var prompt = Cc[
        "@mozilla.org/network/default-auth-prompt;1"
      ].createInstance();
      return prompt.QueryInterface(iid);
    }
    throw Components.Exception("", Cr.NS_NOINTERFACE);
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIRequestObserver",
    "nsIProgressEventSink",
    "nsIInterfaceRequestor",
  ]),
};

var EXPORTED_SYMBOLS = ["UpdateService", "Checker", "UpdateManager"];
